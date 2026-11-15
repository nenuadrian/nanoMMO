# 10. MySQL as the source of truth

## Status

Accepted

## Context

Episode 1 named Redis for the hot state and MySQL as the source of truth, and said SQL was mainly
a preference, which we'd come back to. Episode 5 needed a database for the accounts, and looked at
what was coming after them: items, guilds, friends and trades, data full of relations, where an
item mustn't be duplicated or lost halfway through a trade.

## Decision

- MySQL, from its official image, pinned to a version in `compose.yaml`, with its data on the
  `mysql` volume, so it outlives the container.
- The servers talk to it through MySQL's own C API, with every statement prepared and its values
  sent apart from the SQL, so a name with a quote in it is an odd name and nothing more.
- What the database can hold, it holds: unique keys for an account's name, a character's name in
  its world and a guild's name in its world, foreign keys that take a character's rows with it,
  and a transaction for each change that has to happen whole, such as a bag's rows (episode 40) or
  a guild's (episode 66).
- The code that talks to it stays in a few small classes: `Database`, `Accounts`, `Characters`,
  `Players` and `Guilds`.

## Alternatives considered

- A document database, such as MongoDB, which keeps a whole player, items and all, in one
  document. That suits data read and written whole, and MongoDB has transactions these days, so a
  trade could be safe there too. For a game where players trade and a lot relates to a lot, MySQL
  won (episode 5), and the small classes keep it a choice we could revisit.

## Consequences

- MySQL is too slow to take each tick's changes. Each save was two round trips, to prepare the
  statement and to run it, and a commit that waits for the disk, so the tick went past its 50 ms
  at about fifty players (episode 13). The players reach MySQL through Redis and the saver
  ([0013](0013-save-players-through-redis.md)), and MySQL is up to about five seconds behind the
  game.
- Once there were players to keep, the tables couldn't be made again from a schema file, so they
  change through migrations ([0015](0015-migrations-with-flyway.md)).
- Its client library comes inside MySQL's whole source tree, so it comes from the system's
  packages, and from Homebrew's `mysql-client` on a Mac (episode 5).
- The website reads the same tables for its rankings, and hashes passwords the way the login server
  does, so an account made on either works on both (episode 12).
- Data read and written whole with its player, and never looked up apart, goes in a column as
  JSON, not a table of its own: a player's quests (episode 45), and an item's own state, such as
  its rolls (episode 40).
- MySQL waits fifty seconds for a lock by default, which a tick can't, so our connections wait two
  ([0021](0021-guilds-in-mysql-transactions.md)).
