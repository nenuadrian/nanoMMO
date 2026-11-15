# 15. Change the tables with Flyway's migrations

## Status

Accepted

## Context

Until episode 18, the tables came from `db/schema.sql`, which MySQL runs when it starts on an
empty volume, and at no other time. To add the experience column in episode 10, we deleted the
volume with `docker compose down -v`, and every account with it. That was fine for test accounts,
and not something we can do to players.

## Decision

- The tables are made and changed by migrations: SQL files in `db/`, named
  `V<version>__<description>.sql`, which Flyway runs from its official image, as the `flyway`
  service. It records each version it ran, with a checksum of its file, in
  `flyway_schema_history`.
- It runs before anything else uses MySQL: the servers, the saver, the website and the tests wait
  for it, and don't start if it fails.
- A migration never changes once it has run anywhere. A change goes in a new file, with the next
  version.
- Each change keeps the servers from before it working, as they run on while it does: a new column
  can be `NULL` or has a default, a drop or a rename waits for a later version, and
  `ALGORITHM=INSTANT` refuses a change that would rebuild the table
  ([Saving players and migrations](../saving.md#migrations)).

## Alternatives considered

- An `ALTER TABLE` by hand, on each database. Nothing would record which databases had had it, so
  a laptop, CI and a server would slowly drift apart (episode 18).
- A tool of our own, a loop over files and a table, with a lock. The edge cases, such as a file
  that fails halfway, are where the work is, and that's code we'd look after (episode 18).
- Laravel's migrations, from the website. The website would then own the tables our C++ servers
  depend on, which is the wrong way round (episode 18).
- Liquibase, with each change in XML, YAML or SQL. Flyway takes plain SQL, so what we write is
  what MySQL runs (episode 18).

## Consequences

- Changing a migration that ran is refused, with a checksum mismatch: `ep018/step-2` adds a
  column to V1 on purpose, and step 3 moves it into a V2 of its own.
- Defaults keep the servers from before saving while a deploy migrates: the world is Ember for
  the players from before worlds (V3), their land the Meadows (V4), and their weapon `NULL` (V5).
- Going back a release leaves the database as the newer one left it, as Flyway skips migrations it
  doesn't know, so each migration has to keep the release before it working
  ([Deployment](../deployment.md)).
- A volume from before migrations has the tables and no history, so Flyway won't touch it: it
  needs `baseline`, or `down -v`.
- Flyway's list of the MySQL versions it has verified can trail the one we run. Our migrations are
  plain SQL, and CI runs them on a fresh database on each push, which is what tells us they work.
