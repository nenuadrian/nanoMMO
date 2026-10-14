#!/bin/sh
# Makes a certificate authority of our own, and a certificate it signs for our
# servers, in the folder given (default: this script's). Any names after the
# folder go on the certificate too, such as a server's domain.
set -eu

dir=${1:-$(dirname "$0")}
[ $# -gt 0 ] && shift
mkdir -p "$dir"
cd "$dir"
if [ -f server.crt ]; then
  echo "certificates: $dir has them already"
  exit 0
fi

names="DNS:localhost, IP:127.0.0.1, DNS:login, DNS:server"
for name in "$@"; do
  # An address goes on as an IP, which clients check apart from domain names.
  case $name in
    *:*) names="$names, IP:$name" ;;
    *[!0-9.]*) names="$names, DNS:$name" ;;
    *) names="$names, IP:$name" ;;
  esac
done
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

# The authority's key signs certificates, so nobody else gets to read it.
(umask 077 && openssl ecparam -name prime256v1 -genkey -noout -out ca.key)
openssl req -new -x509 -key ca.key -days 3650 -subj "/CN=nanoMMO authority" \
  -config openssl.cnf -extensions authority -out ca.crt

openssl ecparam -name prime256v1 -genkey -noout -out server.key
# The servers read it as a user of their own, in their containers.
chmod 644 server.key
openssl req -new -key server.key -subj "/CN=nanoMMO servers" \
  -config openssl.cnf -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -set_serial "0x$(openssl rand -hex 16)" -days 825 \
  -extfile openssl.cnf -extensions servers -out server.crt

rm openssl.cnf server.csr
echo "certificates: made a certificate authority and the servers' certificate in $dir"
