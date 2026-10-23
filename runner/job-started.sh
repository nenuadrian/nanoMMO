#!/bin/sh
# Runs before each job the runner takes, from the runner's image, which no pull
# request can change. The runner has the server's Docker, so it takes deploys
# of pushes to main and nothing else, and a job it refuses fails before its
# first step.
if [ "$GITHUB_EVENT_NAME" != push ] || [ "$GITHUB_REF" != refs/heads/main ]; then
  echo "nanommo runner: refused a $GITHUB_EVENT_NAME on $GITHUB_REF," \
    "as it only deploys pushes to main" >&2
  exit 1
fi
