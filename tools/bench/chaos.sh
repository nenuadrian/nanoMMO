#!/usr/bin/env bash
# Chaos experiments: one fault at a time, put into the Compose stack on this
# machine while bots play, and taken out again, with what the metrics, the
# logs and the traces saw, as JSON. It's for staging: it refuses a
# deployment's stack, and production is never where we break things on
# purpose.
#
#   tools/bench/chaos.sh <experiment> [seconds]
#   tools/bench/chaos.sh restore      undoes what a run that was killed left
#
# Each experiment, and how long its fault lasts unless seconds says:
#
#   redis-pause 5       Redis hangs: docker pause freezes it
#   redis-down 7        Redis stops, and starts again
#   mysql-pause 15      MySQL hangs
#   mysql-slow 60       MySQL answers DELAY milliseconds late (50), by netem
#   saver-kill 8        MySQL hangs, the bots leave, the saver takes them to
#                       it, and it's killed mid-flush
#   war-replay 10       the bots go to war, MySQL answers DELAY milliseconds
#                       late (200), the bots leave, and Redis hangs once the
#                       saver has taken their last kills, before it can take
#                       them out of the stream
#   login-pause 20      the login server hangs
#   consul-down 30      Consul stops, and starts again
#   telemetry-pause 60  Alloy hangs, so no spans or logs get through
#
# BOTS bots play (100), arriving over RAMP seconds (30), and PROBES more (10)
# log in while the fault lasts, as players keep coming. KEEP_BOTS=1 leaves
# the BOTS running for the next experiment. Prometheus, Loki, Tempo and
# Grafana are at PROMETHEUS_URL, LOKI_URL, TEMPO_URL and GRAFANA_URL, this
# machine's ports by default.
set -euo pipefail

experiment=${1:?"usage: tools/bench/chaos.sh <experiment> [seconds], or restore"}
bots=${BOTS:-100}
ramp=${RAMP:-30}
probes=${PROBES:-10}
prometheus=${PROMETHEUS_URL:-http://localhost:9090}
loki=${LOKI_URL:-http://localhost:3100}
tempo=${TEMPO_URL:-http://localhost:3200}
grafana=${GRAFANA_URL:-http://localhost:3000}
# The most a fault lasts, whatever seconds says.
longest=300
# How long the game has to be as it should, before and after.
settle=180

say() { echo "chaos: $*" >&2; }

container() { docker compose ps -a -q "$1"; }

# Breaking production on purpose is never the experiment: a deployment's
# stack is started with compose.prod.yaml, which .env names, and Docker on
# another machine is DOCKER_HOST.
refuse_production() {
  local files=""
  if [[ -n $(container redis) ]]; then
    files=$(docker inspect -f \
      '{{index .Config.Labels "com.docker.compose.project.config_files"}}' \
      "$(container redis)")
  fi
  if [[ -n ${DOCKER_HOST:-} || ${COMPOSE_FILE:-} == *compose.prod.yaml* ||
    $files == *compose.prod.yaml* ]] || grep -qs compose.prod.yaml .env; then
    say "this looks like a deployment, not staging: no chaos here"
    exit 2
  fi
}

# Prometheus's answer to a query, now or at a time: its first value, or
# nothing.
query() {
  curl -fsS --max-time 10 --data-urlencode "query=$1" \
    ${2:+--data-urlencode "time=$2"} "$prometheus/api/v1/query" |
    jq -r '.data.result[0].value[1] // empty'
}

# Its values at a time, by a label, as JSON: {"Ember": 1200, ...}.
by() {
  curl -fsS --max-time 10 --data-urlencode "query=$1" \
    --data-urlencode "time=$2" "$prometheus/api/v1/query" |
    jq -c --arg label "$3" \
      '[.data.result[] | {(.metric[$label] // "all"): (.value[1] | tonumber)}]
       | add // {}'
}

# The values of a query from start to end, every two seconds.
range() {
  curl -fsS --max-time 30 --data-urlencode "query=$1" \
    --data-urlencode "start=$2" --data-urlencode "end=$3" \
    --data-urlencode step=2 "$prometheus/api/v1/query_range"
}

# The largest value it had then, or null.
largest() {
  range "$@" | jq '[.data.result[].values[][1] | select(. != "NaN")
                    | tonumber] | max'
}

# The game as it should be: each world up runs its 20 ticks a second, the
# bots' newest worlds are under 200 ms old, they're all connected, no item has
# a copy, and the saver's last good flush is under 15 seconds old. Prints what
# isn't, and fails, if anything isn't.
steady() {
  local up ticking age connected copies behind wrong=()
  up=$(query 'count(max by (world) (up{job="nanommo-server"}) == 1)')
  ticking=$(query 'count(sum by (world)
    (rate(nanommo_tick_duration_seconds_count[10s])) >= 19.5)')
  age=$(query "histogram_quantile(0.99,
    sum by (le) (rate(nanommo_bot_world_age_seconds_bucket{$players}[10s])))")
  connected=$(query 'sum(nanommo_bots_connected)')
  copies=$(query 'max(nanommo_duplicate_items)')
  behind=$(query 'time() - max(nanommo_saver_flushed_timestamp_seconds)')
  [[ ${up:-0} -gt 0 && ${ticking:-0} -ge ${up:-0} ]] ||
    wrong+=("${ticking:-0} of ${up:-0} worlds run 20 ticks a second")
  if ((expected > 0)); then
    [[ $age != NaN ]] && jq -en "${age:-1} < 0.2" >/dev/null ||
      wrong+=("the bots' worlds are ${age:-?} s old")
    [[ ${connected:-0} -eq $expected ]] ||
      wrong+=("${connected:-0} of $expected bots connected")
  fi
  [[ ${copies:-0} -eq 0 ]] || wrong+=("$copies serials held twice")
  jq -en "${behind:-99} < 15" >/dev/null ||
    wrong+=("the saver's last good flush is ${behind:-?} s old")
  ((${#wrong[@]} == 0)) || {
    printf '%s; ' "${wrong[@]}" >&2
    echo >&2
    return 1
  }
}

# Waits until the game's steady, and prints when it was, or gives up.
wait_steady() {
  local deadline=$((SECONDS + settle))
  until steady 2>/dev/null; do
    if ((SECONDS >= deadline)); then
      steady || true
      return 1
    fi
    sleep 2
  done
  date +%s
}

# Bots, in a container of their own, which Prometheus finds as bots. The
# world's age is the BOTS' alone: a swarm waits for each of its logins, so
# the probes' measure their logins as much as their worlds. They join the
# stack as it is: without --no-deps, Compose would start what they depend on,
# or make it again if its configuration had changed, in the middle of an
# experiment. A fifth argument, war, sends them to war, in two guilds.
start_bots() {
  docker compose --progress quiet run -d --rm --no-deps --use-aliases \
    --label nanommo.bench=true --label "nanommo.chaos=$4" \
    bots login server "$1" "$2" "$3" udp ${5:-} >/dev/null
}

stop_bots() {
  docker ps -q --filter "label=nanommo.chaos=$1" | xargs -r docker rm -f \
    >/dev/null
}

# A fault undoes itself a little after it should have ended, should this
# script be killed before it does: each backstop runs a command then, unless
# restore kills it first. Each runs under a name, sh -c's $0, which restore
# finds it by, so that a killed run's backstops go too, before they undo a
# later run's fault.
backstop() {
  nohup sh -c "sleep $((seconds + 15)); $1" nanommo-chaos-backstop \
    >/dev/null 2>&1 &
}

pause() {
  docker pause "$(container "$1")" >/dev/null
  undo="docker unpause $(container "$1")"
  backstop "$undo"
}

# Delays what the service sends by DELAY milliseconds, with the netem image
# in the service's network, which takes the delay off again by itself.
delay() {
  docker run -d --rm --label nanommo.bench=true --cap-add NET_ADMIN \
    --network "container:$(container "$1")" --entrypoint sh "$netem" \
    -c "netem.sh $2 0 && sleep $((seconds + 15)); netem.sh 0 0" >/dev/null
  delayed=$1
}

undelay() {
  docker run --rm --cap-add NET_ADMIN --network "container:$(container "$1")" \
    --entrypoint tc "$netem" qdisc del dev eth0 root 2>/dev/null || true
}

# Whatever a fault left, put back: paused containers thawed, a delay taken
# off, stopped services started, and the backstops gone. Running it twice does
# no harm.
restore() {
  local id stopped
  if [[ -n ${undo:-} ]]; then
    eval "$undo" >/dev/null 2>&1 || true
    undo=""
  fi
  for id in $(docker compose ps -a -q); do
    if [[ $(docker inspect -f '{{.State.Paused}}' "$id") == true ]]; then
      docker unpause "$id" >/dev/null
    fi
  done
  if [[ -n ${delayed:-} ]]; then
    undelay "$delayed"
    docker ps -q --filter label=nanommo.bench --filter ancestor="$netem" |
      xargs -r docker rm -f >/dev/null
  fi
  # Started by Docker, not Compose, which would wait for what they depend on
  # to be healthy, as MySQL isn't for a moment after it's thawed.
  for stopped in $(docker compose ps -a --status exited --format '{{.Service}}' |
    grep -xE 'redis|mysql|consul|login|server|frost|saver|alloy' || true); do
    docker start "$(container "$stopped")" >/dev/null
  done
  pkill -f nanommo-chaos-backstop || true
}

# As the script ends, however it ends.
finish() {
  restore
  stop_bots probes
  [[ -n ${KEEP_BOTS:-} ]] || stop_bots swarm
}

if [[ $experiment == restore ]]; then
  refuse_production
  netem=$(docker compose config --format json | jq -r .name)-netem
  delayed=mysql
  restore
  undelay redis
  stop_bots probes
  stop_bots swarm
  say "restored"
  exit 0
fi

case $experiment in
  redis-pause) seconds=${2:-5} ;;
  redis-down) seconds=${2:-7} ;;
  mysql-pause) seconds=${2:-15} ;;
  mysql-slow) seconds=${2:-60} ;;
  saver-kill) seconds=${2:-8} ;;
  war-replay) seconds=${2:-10} ;;
  login-pause) seconds=${2:-20} ;;
  consul-down) seconds=${2:-30} ;;
  telemetry-pause) seconds=${2:-60} ;;
  *)
    say "no experiment called $experiment"
    exit 1
    ;;
esac
((seconds <= longest)) || {
  say "$seconds s is longer than a fault lasts here, $longest s"
  exit 1
}
refuse_production
trap finish EXIT
trap 'exit 130' INT TERM

docker compose --progress quiet build bots netem
netem=$(docker compose config --format json | jq -r .name)-netem
expected=$bots
# The war kills the worlds counted, and the wars' scores in MySQL, a row for
# each kill the saver recorded.
counted() { query 'sum(nanommo_war_kills_total)'; }
scored() {
  docker compose exec -T mysql mysql -unanommo -pnanommo -N -B nanommo \
    -e "SELECT COUNT(*) FROM war_kills" 2>/dev/null
}
war=""
if [[ $experiment == war-replay ]]; then
  war=war
  counted_from=$(counted)
  scored_from=$(scored)
fi
if [[ -z $(docker ps -q --filter label=nanommo.chaos=swarm) ]]; then
  say "starting $bots bots, over $ramp s${war:+, at war}"
  start_bots "$bots" "$ramp" 1 swarm $war
fi
swarm_ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
  "$(docker ps -q --filter label=nanommo.chaos=swarm | head -1)")
players="instance=\"$swarm_ip:9465\""
say "waiting for the game to be steady"
before=$(wait_steady) || {
  say "the game isn't steady, so the experiment wouldn't say anything"
  exit 1
}

# A minute of the war first, so that the guilds are full, and killing.
if [[ $experiment == war-replay ]]; then
  sleep 60
fi
if ((probes > 0)) && [[ $experiment != saver-kill && $experiment != war-replay ]]; then
  start_bots "$probes" "$seconds" 1001 probes
  expected=$((bots + probes))
fi
say "$experiment for $seconds s"
injected=$(date +%s)
case $experiment in
  redis-pause) pause redis ;;
  mysql-pause) pause mysql ;;
  login-pause) pause login ;;
  telemetry-pause) pause alloy ;;
  redis-down | consul-down)
    service=${experiment%-down}
    docker compose stop "$service" >/dev/null 2>&1
    undo="docker start $(container "$service")"
    backstop "$undo"
    ;;
  mysql-slow) delay mysql "${DELAY:-50}" ;;
  saver-kill)
    # MySQL hangs first, so that no flush saves the bots as they leave. Each
    # leaving marks a bot unsaved in Redis, and the saver's next flush, within
    # its five seconds, takes them out of the set and waits for MySQL with
    # them.
    pause mysql
    stop_bots swarm
    expected=0
    ;;
  war-replay)
    # MySQL answers late, so each flush waits on it, and the bots leave. Once
    # the saver has taken a batch, with kills still in the stream, Redis
    # hangs: MySQL commits the batch, and the saver can't take its kills out
    # of the stream, so the flush after reads them again.
    delay mysql "${DELAY:-200}"
    stop_bots swarm
    expected=0
    docker compose exec -T redis timeout 30 sh -c '
      until [ "$(redis-cli SCARD players:saving)" -gt 0 ] &&
        [ "$(redis-cli XLEN war_kills)" -gt 0 ]; do sleep 0.01; done' || true
    pause redis
    ;;
esac
sleep "$seconds"
if [[ $experiment == saver-kill ]]; then
  docker kill "$(container saver)" >/dev/null
fi
restore
restored_at=$(date +%s)
say "restored after $((restored_at - injected)) s; waiting for the game to be steady again"
if recovered=$(wait_steady); then
  recovered_after=$((recovered - restored_at))
else
  recovered=$(date +%s)
  recovered_after=null
fi
curl -fsS --max-time 5 -H 'Content-Type: application/json' \
  -d "$(jq -n --arg e "$experiment" --argjson t "$injected" \
    --argjson end "$restored_at" '{time: ($t * 1000), timeEnd: ($end * 1000),
      tags: ["chaos", $e], text: "chaos: \($e)"}')" \
  "$grafana/api/annotations" >/dev/null 2>&1 || true

# What the saver lost: each bot's character, as Redis has it and as MySQL
# does, which should be the same once the saver's caught up.
stale=null
if [[ $experiment == saver-kill ]]; then
  sleep 10
  rows=$(docker compose exec -T mysql mysql -unanommo -pnanommo -N -B nanommo \
    -e "SELECT id, x, z, experience FROM players
        WHERE world = 'Ember' AND name LIKE 'bot%' ORDER BY id" 2>/dev/null)
  newer=$(awk '{print "HMGET player:Ember:" $1 " x z experience"}' <<<"$rows" |
    docker compose exec -T redis redis-cli | paste - - -)
  stale=$(paste <(echo "$rows") <(echo "$newer") | awk -F'\t' '
    $5 != "" && ($2 - $5 > 0.01 || $5 - $2 > 0.01 || $3 - $6 > 0.01 ||
      $6 - $3 > 0.01 || $4 != $7) { n++ } END { print n + 0 }')
fi

# Once the saver's caught up, the war kills the worlds counted in the run,
# and what the wars' scores came to, which should be the same.
kills=null
if [[ $experiment == war-replay ]]; then
  sleep 10
  kills=$(jq -n --argjson counted "$(($(counted) - counted_from))" \
    --argjson scored "$(($(scored) - scored_from))" \
    '{counted: $counted, scored: $scored}')
fi

say "injected $injected, restored $restored_at, steady again $recovered"
# Alloy decides which traces to keep ten seconds after their first span, and
# a service that was frozen may not have been read by Prometheus since it was
# thawed, though the game's steady.
sleep 15
from=$((injected - 1))
end=$(date +%s)
count() { by "sum by ($2) ($1)" "$3" "$2"; }
# A world that wasn't there as the window began, as one that started in it,
# has nothing to count from, so it's left out, as one gone by its end is.
ticks=$(jq -n \
  --argjson c0 "$(count nanommo_tick_duration_seconds_count world "$from")" \
  --argjson c1 "$(count nanommo_tick_duration_seconds_count world "$end")" \
  --argjson b0 "$(count 'nanommo_tick_duration_seconds_bucket{le="0.05"}' world "$from")" \
  --argjson b1 "$(count 'nanommo_tick_duration_seconds_bucket{le="0.05"}' world "$end")" \
  --argjson s0 "$(by 'max by (world) (timestamp(nanommo_tick_duration_seconds_count))' "$from" world)" \
  --argjson s1 "$(by 'max by (world) (timestamp(nanommo_tick_duration_seconds_count))' "$end" world)" \
  '$c1 | keys | map(select($c0[.] and $b0[.] and $s0[.]) | {(.): {
       owed: (20 * ($s1[.] - $s0[.]) | round),
       run: ($c1[.] - $c0[.]),
       on_time: ($b1[.] - $b0[.])}}
     | .[] |= . + {missed: (.owed - .on_time), late: (.run - .on_time)})
   | add // {}')
dropped=$(jq -n \
  --argjson d0 "$(by 'sum by (world) (nanommo_players_dropped_total)' "$from" world)" \
  --argjson d1 "$(by 'sum by (world) (nanommo_players_dropped_total)' "$end" world)" \
  '$d1 | with_entries(.value -= ($d0[.key] // 0))')
# Each result is there from the login server's start, so an end without
# counts is one Prometheus couldn't read it at, and the logins are null.
logins=$(jq -n \
  --argjson c0 "$(count nanommo_login_duration_seconds_count result "$from")" \
  --argjson c1 "$(count nanommo_login_duration_seconds_count result "$end")" \
  --argjson b0 "$(count 'nanommo_login_duration_seconds_bucket{le="5.0"}' result "$from")" \
  --argjson b1 "$(count 'nanommo_login_duration_seconds_bucket{le="5.0"}' result "$end")" \
  'if $c0 == {} or $c1 == {} then null
   else $c1 | with_entries(.value -= ($c0[.key] // 0))
     | . + {over_5s: ((([$c1[]] | add) - ([$c0[]] | add))
                      - (([$b1[]] | add) - ([$b0[]] | add)))}
   end')
# And as the bots saw them, from outside.
bot_logins=$(jq -n \
  --argjson c0 "$(count nanommo_bot_login_duration_seconds_count result "$from")" \
  --argjson c1 "$(count nanommo_bot_login_duration_seconds_count result "$end")" \
  '$c1 | with_entries(.value -= ($c0[.key] // 0))')
spans_dropped=$(jq -n \
  --argjson d0 "$(count nanommo_spans_dropped_total job "$from")" \
  --argjson d1 "$(count nanommo_spans_dropped_total job "$end")" \
  '$d1 | with_entries(.value -= ($d0[.key] // 0))')
alerts=$(range 'max by (alertname, alertstate) (ALERTS)' "$from" "$(date +%s)" |
  jq -c '[.data.result[].metric | "\(.alertname) (\(.alertstate))"] | unique')
# Lines on standard error, by service, in the window, and in as long before
# it, as some are written all the time.
errors() {
  curl -fsS --max-time 10 --data-urlencode "query=sum by (service)
    (count_over_time({stream=\"stderr\",
      service=~\"server|frost|login|saver|recorder|bots\"}[$((end - from))s]))" \
    --data-urlencode "time=${1}000000000" "$loki/loki/api/v1/query" |
    jq -c '[.data.result[] | {(.metric.service): (.value[1] | tonumber)}]
           | add // {}' || echo null
}
errors=$(jq -n --argjson during "$(errors "$end")" \
  --argjson before "$(errors "$from")" '{during: $during, before: $before}')
# Tempo 3.1 refuses a search that starts thirty seconds ago to the second: it
# splits a search there, and one part comes out with no length. So a search
# it refuses is asked again from a second earlier.
search() {
  curl -fs --max-time 10 -G --data-urlencode "q=$1" \
    --data-urlencode "start=$2" --data-urlencode "end=$end" \
    --data-urlencode limit=100 "$tempo/api/search"
}
traces() {
  { search "$1" "$from" || search "$1" "$((from - 1))"; } |
    jq -c '[.traces[]? | {id: .traceID, root: "\(.rootServiceName): \(.rootTraceName)",
            ms: (.durationMs // 0)}] | sort_by(-.ms)' || echo '[]'
}
failed_traces=$(traces '{ status = error }')
slow_traces=$(traces '{ traceDuration > 500ms }')
timeline=$(range "sum by (world) (rate(nanommo_tick_duration_seconds_count[4s]))
  or label_replace(histogram_quantile(0.99, sum by (le)
    (rate(nanommo_bot_world_age_seconds_bucket{$players}[4s]))), \"world\", \"age\", \"\", \"\")
  or label_replace(sum(nanommo_bots_connected), \"world\", \"bots\", \"\", \"\")" \
  $((injected - 10)) "$end" |
  jq -c --argjson t0 "$injected" '[.data.result[] | .metric.world as $w
    | .values[] | {t: (.[0] - $t0), ($w): (.[1] | tonumber * 100 | round / 100)}]
    | group_by(.t) | map(add)')

jq -n \
  --arg experiment "$experiment" --argjson seconds "$seconds" \
  --argjson before "$before" --argjson injected "$injected" \
  --argjson restored "$restored_at" --argjson end "$end" \
  --argjson recovered_after "$recovered_after" \
  --argjson ticks "$ticks" --argjson dropped "$dropped" \
  --argjson logins "$logins" --argjson bot_logins "$bot_logins" \
  --argjson stale "$stale" --argjson kills "$kills" \
  --argjson spans_dropped "$spans_dropped" --argjson alerts "$alerts" \
  --argjson errors "$errors" --argjson timeline "$timeline" \
  --argjson failed "$failed_traces" --argjson slow "$slow_traces" \
  --argjson age "$(largest "histogram_quantile(0.99, sum by (le)
    (rate(nanommo_bot_world_age_seconds_bucket{$players}[4s])))" "$from" "$end")" \
  --argjson fewest "$(range 'sum(nanommo_bots_connected)' "$from" "$end" |
    jq '[.data.result[].values[][1] | tonumber] | min')" \
  --argjson connected "$(query 'sum(nanommo_bots_connected)' "$end" | grep . || echo null)" \
  --argjson behind "$(largest 'time() - max(nanommo_saver_flushed_timestamp_seconds)' "$from" "$end")" \
  --argjson copies "$(largest 'max(nanommo_duplicate_items)' "$from" "$end")" \
  '{experiment: $experiment, seconds: $seconds,
    steady_at: ($before | todate), injected_at: ($injected | todate),
    restored_after_s: ($restored - $injected), recovered_after_s: $recovered_after,
    window_s: ($end - $injected + 1),
    ticks: $ticks, players_dropped: $dropped,
    world_age_p99_max_s: $age, bots_connected_fewest: $fewest,
    bots_connected_after: $connected,
    logins: $logins, bot_logins: $bot_logins, saver_behind_max_s: $behind, stale_in_mysql: $stale,
    war_kills: $kills,
    duplicates_max: $copies, spans_dropped: $spans_dropped,
    stderr_lines: $errors, alerts: $alerts,
    error_traces: {count: ($failed | length), first: $failed[:5]},
    slowest_traces: $slow[:3],
    timeline: $timeline}' |
  tee >(jq -r '"chaos: \(.experiment) \(.seconds) s: " +
    (.ticks | to_entries | map("\(.key) missed \(.value.missed) ticks") | join(", ")) +
    "; world age p99 up to \(.world_age_p99_max_s) s; dropped \(.players_dropped);" +
    " recovered after \(.recovered_after_s) s" +
    (if .war_kills then "; the worlds counted \(.war_kills.counted) war kills," +
      " and the scores came to \(.war_kills.scored)" else "" end)' >&2)
