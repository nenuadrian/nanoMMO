# 13. Save the players through Redis, and a saver that takes them to MySQL

## Status

Accepted

## Context

Until episode 13, the game server saved a player to MySQL when they left, and everyone when it
stopped. A crash skipped both, so a player lost everything since they logged in, and the website's
rankings didn't move until a player left. Saving each change to MySQL after each tick fixed the
crash, and took the tick past its 50 ms at about fifty players.

## Decision

- After each tick, the world saves the players who changed into Redis, as a hash for each,
  `player:<world>:<id>`, and its key in the set `players:unsaved`, all of them in one pipeline, so
  it's one round trip a tick however many there are. A bag goes in only when its revision has
  moved, and is marked in `items:unsaved` too (episode 40).
- Redis appends each change to its append-only file, and writes it to disk once a second.
- `nanommo_saver` takes the unsaved players every five seconds, and writes them to MySQL, 500 to a
  statement, each batch a transaction with its bags' rows. If MySQL fails, they go back into the
  set, for the next try.
- A player who joins is read from Redis first, as it's newer, and from MySQL if Redis doesn't have
  them. This is called write-behind, with a fast store first, and the database behind it later.

## Alternatives considered

- Saving each change to MySQL, from the tick. As measured in episode 13, each save is two round
  trips and a commit that waits for the disk, and the tick fell behind at about fifty players.
- One `INSERT` a tick, with every player who changed. Quicker, but the tick would still wait for
  MySQL, which the login server and the website share, so a slow query there, or a table locked
  for a migration, would make the tick late (episode 13).
- Saving on a thread of its own, so the tick doesn't wait. What's waiting in its queue is in the
  game server's memory, and a crash loses it, which is the problem we started with (episode 13).

## Consequences

- A crash of the game server costs at most the tick it happened in, and one of Redis, or of the
  machine, about a second ([Saving players](../saving.md)).
- Saving takes a few milliseconds a tick with hundreds of players, and the server keeps up with as
  many as it did without saving.
- MySQL, and the website's rankings, are up to about five seconds behind the game.
- The saver fails quietly. The game plays on, and the players wait in Redis. So a rule on the
  saver's logs alerts on it ([0009](0009-prometheus-grafana-and-loki.md)).
- A world handing its players over to its next instance saves them into Redis, where the next
  instance loads them from (episode 36).
- Each new thing a player keeps needs a field in the hash, the saver's statement and a migration,
  as skills (episode 34), bags (episode 40) and quests (episode 45) did.
- Redis holds what only one world writes. A guild changes for players other than the one who acts,
  so a copy in Redis could be written back stale, and guilds go to MySQL alone
  ([0021](0021-guilds-in-mysql-transactions.md)).
