#!/bin/sh
# Ships a commit of this repo to a server over SSH, such as debian@203.0.113.7,
# with certificates from a folder tls/certificates.sh made, and starts it there
# with release.sh.
set -eu

host=$1
certificates=$2
ref=${3:-HEAD}
# Players reach the worlds at the server's address, from user@address.
public_host=${host#*@}

# A single SSH connection, so that a deploy right after other SSH commands
# isn't refused: ufw's limit refuses a sixth connection within 30 seconds.
git archive --format=tar --prefix=tls/ --add-file="$certificates/ca.crt" \
  --add-file="$certificates/server.crt" --add-file="$certificates/server.key" \
  --prefix= "$ref" | ssh "$host" "public_host=$public_host" '
  set -eu
  rm -rf nanommo.next
  mkdir nanommo.next
  tar -x -C nanommo.next
  public_host=$public_host nanommo.next/release.sh
'
