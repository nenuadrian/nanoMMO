#!/bin/sh
# Ships a commit of this repo to a server over SSH, such as debian@203.0.113.7,
# with certificates from a folder tls/certificates.sh made, and starts the game
# there with compose.prod.yaml on top of compose.yaml.
set -eu

host=$1
certificates=$2
ref=${3:-HEAD}

# A single SSH connection, so that a deploy right after other SSH commands
# isn't refused: ufw's limit refuses a sixth connection within 30 seconds.
git archive --format=tar --prefix=tls/ --add-file="$certificates/ca.crt" \
  --add-file="$certificates/server.crt" --add-file="$certificates/server.key" \
  --prefix= "$ref" | ssh "$host" '
  set -eu
  rm -rf nanommo.next
  mkdir nanommo.next
  tar -x -C nanommo.next
  if [ -d nanommo ]; then
    cp nanommo/.env nanommo.next/
  else
    # Nobody but our user can read the password and the key.
    (umask 077 && printf "%s\n" COMPOSE_FILE=compose.yaml:compose.prod.yaml \
      "NANOMMO_MYSQL_PASSWORD=$(openssl rand -hex 16)" \
      "NANOMMO_APP_KEY=base64:$(openssl rand -base64 32)" > nanommo.next/.env)
  fi
  # A build that fails, or a test, leaves the release that runs as it is.
  (cd nanommo.next && docker compose build)
  if [ -d nanommo ]; then
    rm -rf nanommo.previous
    mv nanommo nanommo.previous
  fi
  mv nanommo.next nanommo
  cd nanommo
  # A container left running would keep the folder it started from, which is
  # nanommo.previous now.
  docker compose up -d --wait --force-recreate
'
