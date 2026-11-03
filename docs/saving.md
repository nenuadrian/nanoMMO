# Saving players and migrations

After each tick, the game server saves the players whose position, experience or health changed, into
Redis: a hash for each player in each world, `player:<world>:<account id>`, with `world`, `account`,
`x`, `z`, `experience`, `health`, `land`, `weapon`, what's in their main hand, and `skills`, the ids
of the skills they know with commas between them, and its key in a set, `players:unsaved` (see
[Worlds](worlds.md)). A player whose bag's revision has moved gets `items` too, the bag's `Inventory`
message in protobuf's bytes, and their key goes into `items:unsaved` as well. Their quests go in
`quests`, a `Quests` message in protobuf's bytes, the same the client gets. It sends all of it in
one round trip, as a pipeline, so saving takes a few
milliseconds a tick with hundreds of players, and the server keeps up with as many players as it did
without saving. Writing each change to MySQL instead, one statement at a time, takes the tick past
its 50 ms at around 50 players: each save is two round trips, to prepare the statement and to run
it, and a commit that waits for the disk.

`nanommo_saver` takes the unsaved players out of the set every five seconds, and writes them to the
`players` table, 500 to an `UPDATE` of the characters' rows, which the login server made (see
[Characters](accounts.md#characters)), joined to a table of the batch's rows, and their skills to the
`player_skills` table, from V6, a row for each skill a character knows. A learned skill stays
learned, so the saver inserts each of a character's skills each time, and `INSERT IGNORE` skips the
rows MySQL has already. A character's skills go with its row in `players`: deleting it deletes both.
If MySQL fails, they go back into the set, for the next try. A character that was deleted has no
row to join, and one whose hash is gone, or saved by its account before characters, is skipped, so
that one player can't hold back the others. It saves what's left when it stops.

The saver writes a player's bag again only when it's changed, and not each time they take a step:
it takes each player's mark off `items:unsaved` before it reads their hash, so that a bag that
changes in between is marked again, and writes the bags it took off. The `player_items` table, from
V7, has a row for each stack in a bag and each item worn: where it is, `bag 0` to `bag 23` or the
slot, such as `main hand`, the item, how many, and its `state`, what that one has of its own, such
as its rolls, as protobuf's JSON for a stack. A field a later server adds to a stack goes into the
state with no migration. A bag's rows go, and its new rows go in, which has to happen all or not at
all, or a failure between the two would empty it, so each batch is a transaction, with the players'
rows, which the bags' rows need first. `players.items_revision`, from V8, keeps the bag's revision,
so that a player who emptied their bag isn't taken for a new one, and given the kit again.
`players.quests`, from V9, keeps their quests, as protobuf's JSON for the `Quests` message: they're a
few numbers, read and written whole with the player, and never looked up by quest, so they go with
the player's row, with no table of their own. A hash with no quests, from before them or of a player
on none, leaves the ones MySQL has. So the `players` table, and the website's
rankings, are at most about five seconds behind the game.

Redis appends each change to a file, its append-only file, and writes that to disk once a second, so
a crash loses at most about a second. If the game server crashes, Redis still has its players as of
the last tick. When a player joins, the game server reads them from Redis first, as it's newer than
MySQL, and from the `players` table if Redis doesn't have them, and their bag from Redis, or from
`player_items` when the hash has none.

```bash
docker compose exec redis redis-cli HGETALL player:Ember:1
docker compose exec redis redis-cli SCARD players:unsaved
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM player_items'
docker compose logs saver
```

## Migrations

The tables are made and changed by migrations, SQL files in `db/` that [Flyway](https://github.com/flyway/flyway)
runs from its official Docker image. Docker Compose runs it as the `flyway` service, and the login
server, the game server, the saver, the website and the tests wait until it has finished. If it
fails, they don't start.

- Each file is named `V<version>__<description>.sql`, with two underscores, such as
  `V2__players_health.sql`. Flyway runs the versions a database hasn't run yet, in order, and records
  each one in the `flyway_schema_history` table, with a checksum of its file. A file whose name it
  can't read stops it, instead of being skipped.
- Don't change a migration once it has run anywhere. Flyway compares the checksums and refuses to go
  on, as a database that ran the old file wouldn't get the change. A change goes in a new file, with
  the next version.
- Write each change so that the servers from before it still work, as they keep running until the
  new ones replace them: add a column that can be `NULL` or has a default, and drop or rename
  things in a later version, once no code uses them. `ALGORITHM=INSTANT` makes MySQL refuse a change
  it can't make by changing the table's definition alone, instead of quietly rebuilding the table,
  which takes a long time on a big one.
- On a server with real players, back the database up before running new migrations.

```bash
docker compose run --rm flyway info      # which versions have run
docker compose run --rm flyway migrate   # run the new ones
```

A MySQL volume from before the migrations has the tables but no history, and Flyway won't touch it.
`docker compose down -v` starts again from an empty one, without the accounts in it. On a database
you want to keep, Flyway's `baseline` records version 1 without running it.

Flyway 13.10 warns that it has been verified with MySQL up to 9.4, and we run 9.7. Our migrations are
plain SQL, and CI runs them on a fresh database on each push. Its URL turns TLS on, as MySQL's default
authentication, `caching_sha2_password`, takes a password over an encrypted connection, or with
MySQL's RSA key, and it trusts the certificate MySQL makes for itself, which is fine on Compose's own
network.
