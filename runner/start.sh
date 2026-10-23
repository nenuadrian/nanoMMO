#!/bin/sh
# Starts the runner on this server, for the GitHub repository at $1, such as
# https://github.com/nenuadrian/nanoMMO, with a registration token from $2,
# which a new runner needs: gh api -X POST
# repos/nenuadrian/nanoMMO/actions/runners/registration-token --jq .token
set -eu

cd "$(dirname "$0")"
RUNNER_URL=$1 RUNNER_TOKEN=$2 DOCKER_GROUP=$(stat -c %g /var/run/docker.sock) \
  docker compose up -d --build
