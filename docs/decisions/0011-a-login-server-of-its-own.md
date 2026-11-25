# 11. A login server of its own, with Argon2id and one-time tokens

## Status

Accepted

## Context

Episode 5 gave players accounts, and had to keep their passwords so that a leaked database gives
none away. Episode 6 let them log in, to a game server that does everything on one thread, with
50 ms a tick for the whole world ([0008](0008-one-thread-for-the-world.md)).

## Decision

- Passwords are hashed with Argon2id, through libsodium's `crypto_pwhash_str`, at its interactive
  limits, which are 64 MiB and two passes, about 60 ms a hash. The string it gives carries the
  algorithm, its settings and a random salt, and is all the database keeps.
- Logins have a server of their own, `nanommo_login`, on TCP port 7778. It makes accounts, checks
  passwords, and gives the same answer for a wrong name as for a wrong password. Since episode 58
  it opens a session in Redis, which lasts 30 minutes after the last request, and makes, lists and
  deletes characters with it.
- For the character picked, it puts a token in Redis, which works once, within 30 seconds, and the
  client takes it to the character's world. The world takes it out of Redis with `GETDEL`, and
  never sees a password.

## Alternatives considered

- A fast hash, such as SHA-256, which lets an attacker try billions of guesses a second on a
  graphics card. Argon2id takes 64 MiB for each guess, which makes graphics cards a lot less
  helpful. Argon2 won the Password Hashing Competition in 2015, and is OWASP's first choice
  (episode 5).
- bcrypt or scrypt. These are the obvious alternatives, and neither was weighed.
- Checking passwords on the game server. One login would hold up the world for everyone, for
  longer than a tick, and a game server that never sees a password is a separation of concerns too
  (episode 6).
- A signed token, such as a JSON web token, which the game server checks on its own, without the
  trip to Redis. A token in Redis works once, and can be taken back by deleting it (episode 6).
- A table of tokens in MySQL, with a job to delete the old ones, for data we don't want to keep
  (episode 6).

## Consequences

- One login server lets players into every world, which it lists from Consul
  ([0018](0018-find-the-worlds-through-consul.md)), and it's where an old client is turned away
  ([0020](0020-one-version-for-the-whole-game.md)).
- Tokens work once, which bit episode 36, when a world's next instance refused the token its players
  had already said hello with, so a world handing its players over issues a token for each move.
- The password went over the network as typed until episode 14 put TLS on every connection
  ([0014](0014-tls-with-an-authority-of-our-own.md)).
- The website hashes with the login server's settings, so an account made there logs into the
  game, and the other way round (episode 12).
- The UDP hello carries the token again, by when the world has taken it out of Redis, so it can't
  log anyone in ([Accounts, logins and TLS](../accounts.md)).
