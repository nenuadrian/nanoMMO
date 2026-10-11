#!/usr/bin/env bash
set -euo pipefail

per_step=${1:-250}
steps=${2:-8}
ramp=20
hold=30

query() {
  curl -s --data-urlencode "query=$1" http://localhost:9090/api/v1/query \
    | jq '.data.result[0].value[1] | tonumber'
}

stop() {
  docker ps -q --filter label=nanommo.bench | xargs -r docker rm -f > /dev/null
}
trap stop EXIT

docker compose --progress quiet build bots

window="$((hold - 5))s"
quantile="sum by (le) (rate(nanommo_tick_duration_seconds_bucket[$window]))"

for step in $(seq 1 "$steps"); do
  first=$(( (step - 1) * per_step + 1 ))
  docker compose --progress quiet run -d --rm --label nanommo.bench=true bots \
    login server "$per_step" "$ramp" "$first" > /dev/null
  sleep $(( ramp + hold ))

  result=$(jq -n \
    --argjson players "$(query 'nanommo_players_online')" \
    --argjson ticks "$(query "rate(nanommo_tick_duration_seconds_count[$window])")" \
    --argjson p50 "$(query "histogram_quantile(0.5, $quantile) * 1000")" \
    --argjson p99 "$(query "histogram_quantile(0.99, $quantile) * 1000")" \
    --argjson sent "$(query "rate(nanommo_bytes_sent_total[$window]) / 1e6")" \
    '{players: $players, ticks_per_second: $ticks, p50_ms: $p50,
      p99_ms: $p99, sent_mb_per_second: $sent}
     | map_values(. * 10 | round / 10)')
  jq -r '"\(.players) players: \(.ticks_per_second) ticks/s, p99 \(.p99_ms) ms"' \
    <<< "$result" >&2
  echo "$result"
done | jq -s .
