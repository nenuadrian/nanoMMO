# Accounts, logins and TLS

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it opens a session in Redis, `session:<hex>`, which lasts 30 minutes after
  your last request, and sends it to the client with your characters and the worlds that are up.
  The client asks for the rest with the session in place of the password, which it doesn't keep.
- With the session, it makes characters and deletes them (see [Characters](#characters)), and gives
  you a token for the one you pick: it checks the character is yours, puts the token in Redis,
  which works once, within 30 seconds, and sends it to the client.
- The client takes the token to the character's world, a game server, `nanommo_server`. The game
  server takes the token out of Redis, and loads that character, if it's in this world: its name,
  its looks and where it is from the `players` table, with what Redis has on top, as that's newer.
- A client gives up on a request the login server hasn't answered within five seconds,
  `kLoginDeadline` in `protocol/login_client.h`, connecting, shaking hands and the reply together,
  once it has the server's address, and says so. The login server counts each request it answers,
  and how long it took, by what came of it, on port 9467 (see [Reliability](reliability.md)). When
  Consul doesn't answer, it sends the worlds it listed in the last half a minute, as they play on
  without Consul.

## Characters

An account has up to four characters in each world (`kCharactersPerWorld`, in `protocol/limits.h`), each a row in
`players` with an id and a name of its own, which other players see. A name is unique in its world,
and follows an account name's rules: 3 to 16 letters, digits and underscores. `players` has a unique
key on the world and the name, and making a character counts and inserts in one statement, so two
requests at once can't take one name, or the last place, between them. A new character has no bag
yet, so it gets the starting kit as it first enters its world, and the client's preview dresses it in
that kit before then. Deleting one deletes its skills and its bag with it, and deleting an account
deletes its characters. The client, reading the same number, has no New
character once there are four, and the login server turns a fifth away: "You have 4 characters in
Ember already."

A new character comes with the looks its player chose for it (`LoginRequest`'s `looks`, see
[Making a character](gameplay.md#making-a-character)): four places, in the rig's builds and in the
figures file's skins, hair colours and ways to wear the hair, which go into `players`' `build`,
`skin`, `hair_color` and `hair`, from V16, and a fifth, in the client's six players' colours, for
the shirt, which goes into `shirt`, from V17, all in the statement that makes it. A 0.94.0 client
sends no shirt, and its character's shirt stays NULL. The login server reads
no content, so it can't count those lists, and keeps any place that fits its column, up to 255
(`Characters::kMostLook`), and refuses one past that: "That isn't a look there is." Each client
wraps each place round to what its own lists have, and every client in a world has the same
lists, as the figures and the rig are in `ClientHash` (see [Content](content.md)), so a place out
of range draws as a look there is, the same on every screen
([decision 44](decisions/0044-looks-are-chosen-once-and-names-show-over-whoever-is-pointed-at-or-picked.md)).
The characters' listing has each one's looks (`CharacterListing`'s `looks`), which the client's
preview draws it in. A character made without looks, as the bots and the red-team client make
theirs, and every character from before 0.94.0, has NULL in all five, and no looks in its listing,
and its id picks how it looks, as before. One with a NULL shirt alone gets its looks without one,
and its id picks the shirt's colour. Nothing changes a character's looks once it's made. A new
character is in no faction, NULL in `faction`, from V18, until it swears to one in the world (see
[Factions](gameplay.md#factions)).

The world plays a character by its id, so Redis keeps it as `player:<world>:<id>`, and its skills
and bag go by that id (see [Saving players](saving.md)). V10 made characters from the players there
were, one per account and world, named after the account: Ember's kept their account's id, which
their looks come from, and the others' came after the largest account id.

The password, the session and the token go over TLS, so nobody between the client and the servers can read
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
them leaves, and a character can't play twice at once. The server and the players run on the test's
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

Run again, it keeps what's there. Given a name the certificate lacks, or once the certificate has
expired, it signs a new one for the servers, with every name, from the same authority, so the clients' `ca.crt` still trusts it, and the servers
use it once they start again. Delete `tls/*.crt` and `tls/*.key` for a new authority. Git ignores
them, so each copy of the repo has an authority of its own. Its first argument is the folder, so it
refuses an address or a domain there, which would have made a folder of that name.

- The servers load their certificate and key from `NANOMMO_TLS_CERTIFICATE` and `NANOMMO_TLS_KEY`
  (default: `tls/server.crt` and `tls/server.key`, in the repository they were built from, or else
  in the folder they run in, as in Docker).
- The client and the bots trust the certificates the authority in `NANOMMO_TLS_CA` signed (default:
  `tls/ca.crt`), and no others, and check that the server's name is on the certificate. OpenSSL
  checks the name, so a refused handshake says why: the name the certificate lacks, an authority
  of another copy of the repo, or a certificate that's expired.
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
