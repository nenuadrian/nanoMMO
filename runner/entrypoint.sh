#!/bin/sh
# Registers the runner with the repository at RUNNER_URL the first time its
# container starts, with a registration token in RUNNER_TOKEN, and then waits
# for jobs.
set -eu

cd /home/runner
if [ ! -f .runner ]; then
  ./config.sh --unattended --replace --name nanommo --labels nanommo \
    --url "${RUNNER_URL:?the repository's URL}" \
    --token "${RUNNER_TOKEN:?a registration token}"
fi
exec ./run.sh
