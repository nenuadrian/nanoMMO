#!/bin/sh
# Starts a world again from its newest image, such as server or frost, while
# its players play on: its next instance takes them while it starts again, and
# hands them back.
set -eu

world=$1
next=$world-next

docker compose --profile next up -d --wait --no-deps "$next"
# Stopping a world hands its players over to its newest instance in Consul.
docker compose stop "$world"
docker compose up -d --wait --no-deps --force-recreate "$world"
docker compose --profile next stop "$next"
docker compose --profile next rm -f "$next"
