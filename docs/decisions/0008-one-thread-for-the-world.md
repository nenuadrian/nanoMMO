# 8. One thread for the world, on Asio's event loop

## Status

Accepted

## Context

Episode 3 put the server on the network, with clients to accept, a session for each of them to
read and write, a tick twenty times a second, and Ctrl C or `docker stop` to shut down on. All of
them touch the world, and something had to decide which runs when.

## Decision

- The world server runs Asio's `io_context` on one thread, as the login server and the saver do
  too, with standalone Asio, which needs nothing more than a compiler. Its acceptor, its sessions,
  the tick's timer and the signals are handlers on that thread, which run one after another, so
  two of them never touch the world at once, and the world needs no locks.
- Work that would block the thread, and doesn't touch the world, goes to threads of its own:
  Consul's heartbeat, on a pool of one (episode 21), and the storyteller's requests (episode 25).
- Work that would make a tick late goes to another program: checking passwords to the login server
  ([0011](0011-a-login-server-of-its-own.md)), writing to MySQL to the saver
  ([0013](0013-save-players-through-redis.md)), and analytics to the recorder (episode 47).

## Alternatives considered

- A thread for each client, or a world shared between threads behind locks. These are the obvious
  alternatives, and neither was weighed here.
- A server for each of a world's lands, each on a machine of its own, with a trip through a gate
  handed over like a login. Episode 38 runs a world's lands as copies in its one tick loop
  instead, so a trip is a move from one `World` to another within a tick
  ([Worlds and lands](../worlds.md)).

## Consequences

- A world is as fast as a core. With bots spread over the island, it keeps its 20 ticks a second
  up to about 1,200 players on an Apple M4 Pro, and about 700 on a VPS with 2 vCPUs, bots and all
  ([Networking](../networking.md)).
- Anything slow on the thread makes the tick late for everyone, which shaped a lot of later work.
  An Argon2 hash takes longer than a tick (episode 6). Saving to MySQL each tick fell behind at
  about fifty players (episode 13). The TLS handshake runs on the thread, so the keys are ECDSA
  rather than RSA (episode 14). A search for a way across a land looks at 40,000 squares at most
  (episode 52). A MySQL statement waits two seconds at most for a lock, not fifty (episode 66).
- cpp-httplib's sockets block, so the requests to Consul stay off the tick's thread, and a
  heartbeat that finds the last one still waiting skips (episode 21).
- The bots are a thousand players on one thread, so a bot never blocks either: everything it does
  is a chain of handlers.
- The server's test runs the server and its players on the test's own thread, one handler at a
  time, until what it waits for happens (episode 29).
