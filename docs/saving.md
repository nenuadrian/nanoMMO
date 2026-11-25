# Saving players and migrations

After each tick, the game server saves the players whose position, experience or health changed, into
Redis: a hash for each player in each world, `player:<world>:<account id>`, with `world`, `account`,
`x`, `z`, `experience`, `health`, `land`, `weapon`, what's in their main hand, and `skills`, the ids
of the skills they know with commas between them, and its key in a set, `players:unsaved` (see
[Worlds](worlds.md)). A player whose bag's revision has moved gets `items` too, the bag's `Inventory`
message in protobuf's bytes, with their gold in it, and their key goes into `items:unsaved` as
well. Their quests go in `quests`, a `Quests` message in protobuf's bytes, the same the client gets, and
their faction in `faction`, its id, or nothing for none. It sends all of it in
one round trip, as one Lua script, which checks the players' leases and runs the commands with
nothing in between, so that Redis keeps all of it or none of it (`Redis::TransactionIf`, see
[One instance at a time](#one-instance-at-a-time)), and saving takes a few
milliseconds a tick with hundreds of players, and the server keeps up with as many players as it did
without saving. Writing each change to MySQL instead, one statement at a time, takes the tick past
its 50 ms at around 50 players: each save is two round trips, to prepare the statement and to run
it, and a commit that waits for the disk.

A trade changes two players' bags at once, as the second ready arrives, between ticks. Saved apart,
by a save cut off half way or a player who leaves before the next tick, one bag would be kept as it
was and the other as it is, and an item would be made or lost. So a trade that's done saves both
players there and then, in one transaction, before either hears of it, and a player who leaves is
saved as they go, as nothing saves them once they've gone
([decision 28](decisions/0028-a-trade-is-checked-against-the-bags-and-saved-as-one.md)). While the
world hands its players over to its next instance, a player who leaves isn't saved, as the next
instance has loaded them already, from what was saved as the handover began.

Each item that doesn't stack, a sword or a cap, has a serial of its own, `items.Stack.serial`: 64
random bits from `std::random_device`, which `World::Give` gives each one it makes, and a new
player's kit gets as they join. An item saved before serials gets one the first time it's back,
which moves the bag to its next revision, so that it's saved. Potions and bones stack, and have
none. The serial goes with its item through moves and trades, and into its row's `state` in MySQL,
which takes a new field with no migration. So a copy of an item can be told from it.

Each trade that's done has a record: an id of its own (`RandomHex`), the two characters, and what
each gave, as protobuf's JSON for a `TradeSide`, serials and all. It goes into a Redis stream,
`trades`, in the same script as the trade's two bags, and the saver takes it to MySQL with them
(see below).

`nanommo_saver` takes the unsaved players out of the set every five seconds, and writes them to the
`players` table, 500 to an `UPDATE` of the characters' rows, which the login server made (see
[Characters](accounts.md#characters)), joined to a table of the batch's rows, and their skills to the
`player_skills` table, from V6, a row for each skill a character knows. A learned skill stays
learned, so the saver inserts each of a character's skills each time, and `INSERT IGNORE` skips the
rows MySQL has already. A character's skills go with its row in `players`: deleting it deletes both.
A character that was deleted has no row to join, and one whose hash is gone, or saved by its
account before characters, is skipped, so that one player can't hold back the others. It saves
what's left when it stops.

A batch moves out of `players:unsaved` into `players:saving`, in one script, and stays there until
MySQL has committed it; then that set goes. If MySQL fails, or the saver dies with a batch, as one
killed while MySQL hangs does, the next flush puts it back into the unsaved set first, and saves it
then: a reliable queue, at least once, as the trades stream is. Before episode 89, the batch came
out of the set with `SPOP`, and only an error from MySQL put it back, so a saver killed mid-flush
lost its batch, and those players' rows stayed behind Redis's for good, with nothing to say so
([Reliability](reliability.md#what-the-experiments-found)). There's one saver at a time, which it
takes for granted. `nanommo_unsaved_players` counts who's in either set, once, and
`nanommo_saver_flushed_timestamp_seconds` is when the last good flush ended, whose age is the saves'
objective.

The saver writes a player's bag again only when it's changed, and not each time they take a step:
it moves each player's mark from `items:unsaved` into `items:saving` before it reads their hash, so
that a bag that changes in between is marked again, and writes the bags it moved. The `player_items` table, from
V7, has a row for each stack in a bag and each item worn: where it is, `bag 0` to `bag 23` or the
slot, such as `main hand`, the item, how many, and its `state`, what that one has of its own, such
as its rolls, as protobuf's JSON for a stack. A field a later server adds to a stack goes into the
state with no migration. A bag's rows go, and its new rows go in, which has to happen all or not at
all, or a failure between the two would empty it, so each batch is a transaction, with the players'
rows, which the bags' rows need first. `players.items_revision`, from V8, keeps the bag's revision,
so that a player who emptied their bag isn't taken for a new one, and given the kit again, and
`players.gold`, from V12, their gold, which is in the bag's message and goes with its revision, so
the saver writes it as it does the bag's rows, and a bag that hasn't changed keeps the gold MySQL
has.
`players.quests`, from V9, keeps their quests, as protobuf's JSON for the `Quests` message: they're a
few numbers, read and written whole with the player, and never looked up by quest, so they go with
the player's row, with no table of their own. A hash with no quests, from before them, leaves the
ones MySQL has. Quests that have changed carry their revision, so a player who's given up all they
were on saves as none. So the `players` table, and the website's
rankings, are at most about five seconds behind the game.

A player's faction, `players.faction`, from V18, a `TINYINT UNSIGNED` that's NULL for none, changes
in the world, as they swear to one at its herald ([Gameplay](gameplay.md#factions)), so it's saved
as their skills are: in the tick's save into Redis, and by the saver from there. A faction is for
good, so the saver writes one only where the row has none, `COALESCE(players.faction,
saved.faction)`, and nothing, not a hash with none, from before factions, nor one with another,
switches it. A world reads MySQL's as the player enters, or Redis's, while the saver hasn't taken it
there yet.

A character's looks, `players.build`, `skin`, `hair_color` and `hair`, from V16, are in MySQL
alone: the login server writes them as it makes the character (see
[Characters](accounts.md#characters)), they never change, and the game server reads them with the
rest of the character's row as it loads them, so neither Redis nor the saver has them. Each is a
`TINYINT UNSIGNED`, NULL for a character from before there were looks to choose, whose id picks
them.

In each batch, the oldest trades in the `trades` stream go first, up to 250, each with both its players,
so that MySQL has a trade's record and both bags from after it, or none of it. The saver inserts
each into the `trades` table, from V13, in the batch's transaction, with the time Redis took it,
from its entry's id. The trade's id is the table's primary key, and `INSERT IGNORE` skips one that's
there already, so a batch that MySQL committed without the saver hearing so, as when the
connection drops, and that it tries again, records the trade once. A record leaves the stream once
it's in MySQL. A character who's deleted leaves their trades on record.

After each flush, the saver counts the serials that more than one item in `player_items` has, as
only an item and its copy can share one, by the `serial` column, from V13, which MySQL takes from
each row's `state`, with an index. It serves the count to Prometheus as `nanommo_duplicate_items`,
and `ItemsDuplicated` fires once it's been above zero for a minute (see
[Alerts](monitoring.md#alerts)).

Redis appends each change to a file, its append-only file, and writes that to disk once a second, so
a crash loses at most about a second. If the game server crashes, Redis still has its players as of
the last tick. When a player joins, the game server reads them from Redis first, as it's newer than
MySQL, and from the `players` table if Redis doesn't have them, and their bag from Redis, or from
`player_items` when the hash has none.

```bash
docker compose exec redis redis-cli HGETALL player:Ember:1
docker compose exec redis redis-cli SCARD players:unsaved
docker compose exec redis redis-cli SCARD players:saving
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM player_items'
docker compose exec redis redis-cli XRANGE trades - +
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM trades'
docker compose logs saver
```

## One instance at a time

A world runs as two instances while it hands its players over (see
[Deploying without downtime](deployment.md#deploying-without-downtime)), and each instance knows
only its own players, so a character could say hello to both, with a token for each. Each would load
them, and save them as it had them, over what the other saved: `twice`, the red-team client, gave
its sword away in a trade on one instance, and the other saved its bag from before the trade, so
both players had the sword (see
[What a modified client can try](networking.md#what-a-modified-client-can-try)). So a character
plays on one instance of its world at a time, the one that has their lease:

- The lease is a key in Redis, `playing:<world>:<character>`, which holds the instance's id, as
  Consul names it, its world's and its port's, and a fencing number from the world's counter,
  `fencing:<world>`, which is higher for each lease: `Ember-7777 42`.
- An instance takes it as the player says hello, before it loads them, in a Lua script, which Redis
  runs with nothing else in between: if no instance has it, it's this one's, with the next number,
  for ten seconds (`Players::kLease`). If another has it, the player's turned away: "refused player
  7: they're playing on Ember-7777". A refused hello takes no number.
- Each second, the instance renews all its players' leases for another ten seconds, in one script,
  each only while it's still the one the instance took. An instance that crashes, or stops without
  letting go, keeps its players for ten seconds at most.
- A player who leaves is saved, and lets go of their lease, with a script that deletes it only if
  it's still this instance's, so that they can play on another at once.
- In a handover, the instance saves its players, hands each lease to the next instance, with a new
  number, in one script, and only then tells the clients where to go. The next instance takes a
  lease that's been handed to it as its own.

A lease runs out if its instance stops renewing it, as one that stalls for ten seconds does, a
process stopped or a machine that's swapping, and then another instance can take the player, with a
higher number, and load them. The first doesn't know until it next talks to Redis, so the leases
fence its saves: `Players::Save` writes the players only if each one's lease is still the one its
instance took, instance and number, which the script that writes them checks first
(`Redis::TransactionIf`), and writes none of it if one isn't. An instance that finds a player's lease
isn't its own any more, as it saves them or renews the leases, drops them without saving them, and
says so: "couldn't save mordred, as their lease isn't Ember-7777 2 any more". The rest of a tick's
save goes again without them. A trade whose save is refused drops both its players, who come back
as Redis has them, from before it
([decision 31](decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md)).

```bash
docker compose exec redis redis-cli KEYS 'playing:*'
docker compose exec redis redis-cli GET playing:Ember:1
```

## Guilds

A guild changes for players who aren't the one who acts: a leader who leaves makes someone else the
leader. A player's own world saves them, in Redis, and a guild in there could be written back by
any world that had it as it was. So guilds are in MySQL alone, and each change is a transaction.

| Table | Has |
|---|---|
| `guilds` | Each guild's id, world and name, with a unique key on the world and the name, which MySQL's default collation compares without case |
| `guild_members` | Each member, by their character's id, its primary key, so a character is in one guild at most, with their grade: 0 a member, 1 an officer, 2 the leader. A deleted character's row goes with it, and a guild's rows with the guild |

- Founding inserts the guild and its leader in one transaction, so a character in a guild
  already can't take a name.
- Every change to a guild locks its row in `guilds` first, `SELECT ... FOR UPDATE`, then the
  member's own (`LockFor` in `server/guilds.cc`), so changes to one guild happen one at a time, and
  in the same order, which can't deadlock. A test runs a kick and a leave at once on two
  connections, and whichever goes second finds the player gone.
- The roster is read from MySQL once a tick at most for each guild, and each world adds who's
  playing there, and their level as it is.
- Invitations are in Redis, as `guild_invite:<character>`, holding the guild's id. `SET ... NX EX
  60` lets one guild ask a player at a time, from any instance, and lets it run out by itself.
  Joining reads it, and deletes it once the player has joined or can't, so that a MySQL that fails
  for a moment leaves it to try again, and saying no deletes it, so that another guild can ask.
- The world server reads a player's guild as they join, and keeps the guilds of its players.
- A statement waits two seconds at most for a lock another transaction holds, not MySQL's fifty,
  as a world's tick waits for it, and fails then as `Aborted`. The player is told to try again.

```bash
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM guilds'
docker compose exec redis redis-cli KEYS 'guild_invite:*'
```

## Guild wars

A war between two guilds ([Gameplay](gameplay.md#guild-wars)) changes both, so it's in MySQL too,
in `guild_wars`, from V14: the world, the guild that asked, `challenger`, and the one that said
yes, `challenged`, and when it started and when it ends. A war's rows go with either guild, and
an ended war stays on record. Its kills are rows of their own, in `war_kills`, from V15 (below).

- An ask is in Redis, as `war_ask:<world>:<guild>:<enemy>`, holding who asked, set with `SET ... NX
  EX 60`, as an invitation is, so a guild asks another once until it's answered, and the ask runs
  out by itself. The other guild's leader asking back reads it, and deletes it once the war has
  started or can't.
- The yes is a transaction, which locks both guilds' rows, the lower id first, then the leader's
  own, as every change to a guild locks its row before any member's, and checks that neither guild
  is at war, as a guild is in one at a time. A test has two leaders say yes at once, each to the
  other's ask, on two connections, and the second finds the first's war.
- The world server reads the world's wars that haven't ended every five seconds, in the tick, with
  each side's name and score.
- Leaving a guild, removing someone from it and disbanding it each check, in their transaction,
  after the guild's row is locked, that it isn't at war (`NotAtWar` in `server/guilds.cc`), and are
  refused while it is. The yes locks both guilds' rows, so a war can't start between the check and
  the change. Deleting a character leaves its guild the same way, so it's refused too.

Each kill in a war that scores goes into another stream, `war_kills`, in the same script as the
tick's save: an id of its own, which the world server gives it as it takes it from the world
(`RandomHex`, as a trade's record has), the war, the world, the killer, the one killed and the
killer's guild, whose side it counts for. In each batch, the saver reads the oldest kills, up to
500, as it reads the trades, inserts each as a row of `war_kills`, with the millisecond Redis took
it, in the batch's transaction, and takes them out of the stream once MySQL has them. A war's
score is how many of its rows each side has, by the killer's guild, which a key on the war and the
guild counts. So it's a few seconds behind the game: the saver's flush, every five seconds, and the
world's next read of the wars. A kill whose save Redis doesn't take, as when it can't be reached,
isn't counted.

Like the trades, the kills reach MySQL at least once. A batch that MySQL committed without the
saver hearing so, or whose kills the saver couldn't take out of the stream after, as when Redis
hangs in between, is read again by the next flush. Before episode 92, the saver added each batch's
kills to counts in `guild_wars`, `challenger_kills` and `challenged_kills`, so a batch read again
was counted again: `tools/bench/chaos.sh war-replay` has Redis hang just then, and the scores came
to more than the kills the worlds counted, three times out of three
([Reliability](reliability.md#a-wars-kills-counted-twice)). Now `INSERT IGNORE` skips a kill whose
id is there already, and one whose war has gone since, so each kill counts once. The counts' columns
aren't written any more, and stay, as the servers of 0.91.0 read them until they're replaced. A kill
from one of those, during a deploy, has no id, and goes by its entry's in the stream.

A player scores for a war once every five minutes at most: the world server remembers when each
victim last scored, by the war, and hands the saver only the kills that score
(`Scoring`, `server/wars.h`). It's the instance's memory, so the next instance starts with none.
`nanommo_war_kills_total` counts the kills each world hands the saver, which is what the scores in
MySQL should add up to.

```bash
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM guild_wars'
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM war_kills'
docker compose exec redis redis-cli KEYS 'war_ask:*'
docker compose exec redis redis-cli XRANGE war_kills - +
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
