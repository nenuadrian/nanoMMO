#!/bin/sh
set -eu

name=${1:-server}
seconds=${2:-10}
perf record -F 997 -g -p "$(pidof nanommo_server)" -o "/profile/$name.data" \
  -- sleep "$seconds"
perf script -F -period -i "/profile/$name.data" \
  | stackcollapse-perf.pl > "/profile/$name.folded"
flamegraph.pl --title "nanommo server: $name" "/profile/$name.folded" \
  > "/profile/$name.svg"
echo "profile.sh: flame graph in profile/$name.svg"
