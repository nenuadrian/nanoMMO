#!/bin/sh
# Makes a certificate authority of our own, and a certificate it signs for our
# servers, in the folder given (default: the repo's tls). Any names after the
# folder go on the certificate too, such as a server's domain or address. Run
# again with a name the certificate lacks, or once it's expired, it signs a new
# one with every name, from the same authority, so the clients' ca.crt still
# trusts it.
set -eu
# Git for Windows' sh would take /CN=... for a path, and turn it into one.
export MSYS_NO_PATHCONV=1

if [ $# -gt 0 ]; then
  dir=$1
  shift
else
  # The servers look for them in tls, at the top of the repo.
  cd "$(dirname "$0")/.."
  dir=tls
fi
# A name where the folder goes would quietly make a folder of that name.
case $dir in
  */*) ;;
  *.*)
    if [ ! -d "$dir" ]; then
      echo "certificates: the folder comes first, then the names:" \
        "scripts/certificates.sh tls $dir" >&2
      exit 1
    fi
    ;;
esac
mkdir -p "$dir"
cd "$dir"

names="DNS:localhost, IP:127.0.0.1, DNS:login, DNS:server"
for name in "$@"; do
  # An address goes on as an IP, which clients check apart from domain names.
  case $name in
    *:*) names="$names, IP:$name" ;;
    *[!0-9.]*) names="$names, DNS:$name" ;;
    *) names="$names, IP:$name" ;;
  esac
done

if [ -f server.crt ]; then
  # The names it has, written as we write them: openssl prints "IP Address:".
  have=$(openssl x509 -in server.crt -noout -text |
    sed -n '/Subject Alternative Name/{n;s/^ *//;s/IP Address:/IP:/g;p;}')
  lacking=$(echo "$names" | tr ',' '\n' | sed 's/^ *//' | while read -r name; do
    case ", $have, " in
      (*", $name, "*) ;;
      (*) echo "$name" ;;
    esac
  done)
  # One that's run out is signed again, with the names it has.
  expired=
  openssl x509 -in server.crt -noout -checkend 0 > /dev/null || expired=yes
  if [ -z "$lacking" ] && [ -z "$expired" ]; then
    echo "certificates: $dir has them already, for $have"
    exit 0
  fi
  if [ ! -f ca.key ]; then
    echo "certificates: $dir has no ca.key to sign a new certificate with" >&2
    exit 1
  fi
  # Each name is a word of its own, so the shell splits them for printf.
  [ -n "$lacking" ] && names="$have$(printf ', %s' $lacking)" || names=$have
fi

cat > openssl.cnf <<END
[req]
distinguished_name = name
[name]
[authority]
basicConstraints = critical, CA:true
keyUsage = critical, keyCertSign
[servers]
basicConstraints = critical, CA:false
keyUsage = critical, digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = $names
END

if [ ! -f ca.key ]; then
  # The authority's key signs certificates, so nobody else gets to read it.
  (umask 077 && openssl ecparam -name prime256v1 -genkey -noout -out ca.key)
  openssl req -new -x509 -key ca.key -days 3650 -subj "/CN=nanoMMO authority" \
    -config openssl.cnf -extensions authority -out ca.crt
  made="a certificate authority and the servers' certificate"
else
  made="the servers' certificate, from the authority it had,"
  again="; start the servers again to use it"
fi

openssl ecparam -name prime256v1 -genkey -noout -out server.key
# The servers read it as a user of their own, in their containers.
chmod 644 server.key
openssl req -new -key server.key -subj "/CN=nanoMMO servers" \
  -config openssl.cnf -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -set_serial "0x$(openssl rand -hex 16)" -days 825 \
  -extfile openssl.cnf -extensions servers -out server.crt

rm openssl.cnf server.csr
echo "certificates: made $made in $dir, for $names${again:-}"
