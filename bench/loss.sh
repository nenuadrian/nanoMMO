#!/usr/bin/env bash
set -euo pipefail

bots=${1:-200}
delay=50
hold=60
ready_timeout=120

query() {
  curl --fail --silent --show-error --max-time 10 \
    --data-urlencode "query=$1" http://localhost:9090/api/v1/query \
    | jq -er '.data.result[0].value[1] | tonumber'
}

wait_for_query() {
  local expression=$1
  local deadline=$((SECONDS + ready_timeout))
  local value
  until value=$(query "$expression"); do
    if (( SECONDS >= deadline )); then
      echo "timed out waiting for Prometheus query: $expression" >&2
      return 1
    fi
    sleep 2
  done
  printf '%s' "$value"
}

stop() {
  docker ps -q --filter label=nanommo.bench | xargs -r docker rm -f > /dev/null
  docker compose --progress quiet run --rm netem 0 0 > /dev/null
}
trap stop EXIT

docker compose --progress quiet build bots netem

docker compose --progress quiet run -d --rm --use-aliases \
  --label nanommo.bench=true bots login server "$bots" 10 > /dev/null

window="$((hold - 10))s"
age="sum by (le) (rate(nanommo_bot_world_age_seconds_bucket[$window]))"
echo "waiting for bot metrics" >&2
wait_for_query "histogram_quantile(0.5, $age) * 1000" > /dev/null

for loss in 0 1 2 5 10; do
  docker compose --progress quiet run --rm netem "$delay" "$loss" > /dev/null
  sleep "$hold"

  p50=$(wait_for_query "histogram_quantile(0.5, $age) * 1000")
  p99=$(wait_for_query "histogram_quantile(0.99, $age) * 1000")
  result=$(jq -n \
    --argjson loss "$loss" \
    --argjson p50 "$p50" \
    --argjson p99 "$p99" \
    '{loss_percent: $loss, p50_ms: $p50, p99_ms: $p99}
     | map_values(. * 10 | round / 10)')
  jq -r '"\(.loss_percent)% loss: p50 \(.p50_ms) ms, p99 \(.p99_ms) ms"' \
    <<< "$result" >&2
  echo "$result"
done | jq -s .
