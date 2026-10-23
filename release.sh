#!/bin/sh
# Starts the release in the folder this script is in, nanommo.next on a server,
# such as a commit deploy.sh sent: it builds the release's images, and if they
# build and the tests pass, the release becomes nanommo, the one that runs.
set -eu

cd "$(dirname "$0")/.."
if [ -d nanommo ]; then
  cp nanommo/.env nanommo.next/
  # A release without certificates keeps the ones the release before has.
  if [ ! -f nanommo.next/tls/server.key ]; then
    cp nanommo/tls/ca.crt nanommo/tls/server.crt nanommo/tls/server.key \
      nanommo.next/tls/
  fi
else
  # The first release on a server needs the server's address, from deploy.sh.
  : "${public_host:?the first release on a server comes from deploy.sh}"
  # Nobody but our user can read the password and the key.
  (umask 077 && printf "%s\n" COMPOSE_FILE=compose.yaml:compose.prod.yaml \
    "NANOMMO_MYSQL_PASSWORD=$(openssl rand -hex 16)" \
    "NANOMMO_APP_KEY=base64:$(openssl rand -base64 32)" > nanommo.next/.env)
fi
grep -q "^NANOMMO_PUBLIC_HOST=" nanommo.next/.env ||
  echo "NANOMMO_PUBLIC_HOST=$public_host" >> nanommo.next/.env
# A build that fails, or a test, leaves the release that runs as it is.
(cd nanommo.next && docker compose --profile next build)
if [ -d nanommo ]; then
  rm -rf nanommo.previous
  mv nanommo nanommo.previous
fi
mv nanommo.next nanommo
cd nanommo
# A container left running would keep the folder it started from, which is
# nanommo.previous now. The worlds start again through their next instances,
# one at a time, so that their players play on.
docker compose up -d --wait --force-recreate \
  $(docker compose config --services | grep -vx -e server -e frost)
./handover.sh server
./handover.sh frost
