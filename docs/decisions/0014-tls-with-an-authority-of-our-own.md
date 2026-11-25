# 14. TLS 1.3 between the clients and the servers, with an authority of our own

## Status

Accepted

## Context

In episode 14, tcpdump showed a login going over the network with the password as it was typed,
and the token on its way back. Anyone between a player and our servers could read both.

## Decision

- Everything the clients and the bots send the servers over TCP goes through TLS 1.3, and nothing
  older, as both ends are ours. TLS covers logins on port 7778, and the game's connection on 7777,
  with the token, the input and, over TCP, the world. It's OpenSSL, through Asio's `ssl::stream`.
- The certificates come from a certificate authority of our own, which `tls/certificates.sh`
  makes, with ECDSA keys on P-256. It signs one certificate for the servers, with the names
  they're reached by. The clients and the bots trust that authority and no other, and check the
  server's name is on the certificate.
- Git ignores the keys, so each copy of the repository has an authority of its own, and the
  authority's key stays off the servers.
- The world over UDP isn't encrypted.

## Alternatives considered

- A public authority, such as Let's Encrypt, as browsers trust. It checks that you control a
  domain, so it signs nothing for `localhost` or the names inside Docker, and we ship the client,
  so it can carry our own authority (episode 14).
- RSA keys of 2,048 bits, which a lot of servers still use. Their signatures are a lot slower than
  ECDSA's, and the game server shakes hands on the thread that runs the tick (episode 14).
- DTLS, which is TLS for datagrams, or encrypting each datagram with a key agreed over TLS, for the
  world over UDP. Either is more work on each datagram, for what every player sees in the game
  anyway (episode 14).

## Consequences

- A handshake costs the server well under a tenth of a millisecond, and a round trip, and the
  encryption runs at gigabytes a second with the processor's AES instructions (episode 14).
- The certificate has to name each host a client connects to, which bit more than once, with the
  Compose names `login` and `server` (episode 14), an address as an IP name rather than a DNS one
  (episode 19), and another machine's address, which `certificates.sh` adds from 0.65.0. A refused
  handshake now says which of these it is.
- Someone who can see a player's traffic could take the UDP hello's token, which can't log anyone
  in, and have that player's worlds sent elsewhere, as they could drop them anyway
  ([Accounts, logins and TLS](../accounts.md)).
- A deploy sends the servers their certificate and key, and the players' clients need `ca.crt`
  ([0016](0016-deploy-with-compose-to-one-server.md)).
- Inside Compose's network, the servers talk to Consul over plain HTTP (episode 21).
