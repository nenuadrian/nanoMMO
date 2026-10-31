# Accounts, logins and TLS

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it puts a token in Redis, which works once, within 30 seconds, and sends
  it to the client.
- The client takes the token to the world you picked, a game server, `nanommo_server`. The game
  server takes the token out of Redis, and loads your player in that world: your name from the
  `accounts` table, and where you are and your experience from Redis, or from the `players` table if
  Redis doesn't have you.

The password and the token go over TLS, so nobody between the client and the servers can read
them (see [TLS](#tls)).

The servers find MySQL, Redis and Consul on `127.0.0.1`, or wherever `NANOMMO_MYSQL_HOST`,
`NANOMMO_REDIS_HOST` and `NANOMMO_CONSUL_HOST` point. The tests that need them skip themselves
unless those are set:

```bash
NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 NANOMMO_CONSUL_HOST=127.0.0.1 \
  ./build/bin/nanommo_server_tests
# Or all in Docker, next to MySQL and Redis.
docker compose run --rm tests
```

The world server's test starts a server inside the test, on a port the system picks, and has
players join it over TLS, with the protocol tests' certificates: they see each other until one of
them leaves, and an account can't play twice at once. The server and the players run on the test's
thread, one handler at a time, until what the test waits for happens, or for five seconds at most.

MySQL keeps its data in the `mysql` volume, and Redis in the `redis` volume, so they survive `docker
compose down`. Flyway makes the tables, and changes them, before anything else uses MySQL (see
[Migrations](saving.md#migrations)). `docker compose down -v` deletes the volumes, and with them all the
accounts and players.

## TLS

Everything the clients and the servers send over TCP goes through TLS 1.3, with OpenSSL, through
Asio's `ssl::stream`: logins on port 7778, and the game's connection on port 7777, with the token,
the input and, over TCP, the world. Both ends are ours, so they speak TLS 1.3 and nothing older.

The certificates come from a certificate authority of our own. `tls/certificates.sh` makes its key,
`tls/ca.key`, and its certificate, `tls/ca.crt`. Then a key for the servers, `tls/server.key`, and a
certificate the authority signs for it, `tls/server.crt`, for `localhost`, `127.0.0.1`, and `login` and
`server`, their names in Docker Compose. The keys are ECDSA, on the P-256 curve. Names after the folder
go on the certificate too, such as a server's domain, or its address, which goes on as an IP name:

```bash
tls/certificates.sh tls play.example.com 203.0.113.7
```

It leaves certificates that are there already alone, so delete `tls/*.crt` and `tls/*.key` to make new
ones. Git ignores them, so each copy of the repo has an authority of its own.

- The servers load their certificate and key from `NANOMMO_TLS_CERTIFICATE` and `NANOMMO_TLS_KEY`
  (default: `tls/server.crt` and `tls/server.key`).
- The client and the bots trust the certificates the authority in `NANOMMO_TLS_CA` signed (default:
  `tls/ca.crt`), and no others, and check that the server's name is on the certificate.
- In Docker Compose, the `certificates` service makes them, the first time, before the servers start,
  and the servers and the bots get `tls/` read-only.

Only the authority's key can sign certificates the clients trust, so on a real server, keep it
somewhere else: the servers need `server.crt` and `server.key`, and the players' clients `ca.crt`.

The world over UDP isn't encrypted: it holds what each player sees in the game anyway. Its first
datagram, the hello, carries the login token, which the game server took out of Redis by then, so it
can't log anyone in. Someone who can see your traffic could use it to send your world updates
elsewhere, which someone in that position could stop anyway, by dropping them.

To see the handshake and the certificate, with the login server running:

```bash
openssl s_client -connect localhost:7778 -CAfile tls/ca.crt < /dev/null
```
