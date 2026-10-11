#!/usr/bin/env bash
set -euo pipefail

delay=${1:-50}
loss=${2:-0}

# Without this, netem sees big writes before they're cut into packets, and
# drops them whole.
ethtool -K eth0 tso off gso off
# By default, netem drops anything past a thousand waiting packets, and a
# busy server goes past that.
tc qdisc replace dev eth0 root netem delay "${delay}ms" loss "${loss}%" \
  limit 100000
tc qdisc show dev eth0
