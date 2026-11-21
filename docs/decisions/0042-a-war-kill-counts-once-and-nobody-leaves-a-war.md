# 42. A war kill counts once, a victim scores once every five minutes, and nobody leaves a war

## Status

Accepted

## Context

Episode 91 gave guilds wars, with a score: each kill went into Redis's `war_kills` stream with the
tick's save, and the saver added each batch's kills to two counts in `guild_wars`
([0041](0041-guilds-at-war-hurt-each-other-anywhere.md)). The saver is at least once: a batch stays
in Redis until MySQL has it, and one that MySQL committed without the saver hearing so, or whose
kills it couldn't take out of the stream after, is read again by the next flush
([Saving players](../saving.md)). A trade's record was safe from that from the start, as it has an
id of its own, which `INSERT IGNORE` skips. A war's counts weren't. Episode 92 measured it with a
chaos experiment, `bench/chaos.sh war-replay`: 100 bots at war, MySQL slowed so that a flush waits
on it, the bots gone, and Redis frozen as soon as the saver had taken a batch with kills. Three runs
out of three, the scores came to more than the kills the worlds counted: 200 for 196, 265 for 264,
281 for 267.

A war's score also invites two exploits that a score has to close, with as little rule as will
do. A player can feed kills to a friend in the enemy guild: the red-team client's `farm` killed its
partner, who stood still and came back each time, fifteen times in six minutes, and the score went
15 to 0. And a losing side can leave the war: its members leave the guild, so there's nobody to
kill, or its leader disbands it, which takes the war's rows with it through the cascade, score and
all.

## Decision

- Each kill has an id of its own, from `RandomHex`, which the world server gives it as it takes it
  from the world, and which goes into the stream with it. The saver inserts each kill as a row of
  `war_kills`, from V15, with `INSERT IGNORE`, in the batch's transaction: the id, the war, the
  world, the killer, the victim, the killer's guild, and the millisecond Redis took it. A kill read
  again adds nothing, and nor does one whose war has gone since.
- A war's score is how many of its rows each side has, by the killer's guild, counted on a key on
  the war and the guild (`Guilds::Wars`). `guild_wars`' counts aren't written any more, and stay in
  the table, as a migration keeps the release before it working
  ([0015](0015-migrations-with-flyway.md)). A kill from a server before 0.92.0, during a deploy, has
  no id, and goes by its entry's in the stream.
- A victim scores for a war once every five minutes at most (`Scoring::kOnceEvery`), whoever kills
  them. The kill still happens: they die, both guilds hear of it, and it's a death in the analytics.
  It just doesn't go to the saver. The world server remembers when each victim last scored, by the
  war, in memory (`Scoring`, `server/wars.h`).
- While a guild is at war, nobody leaves it, nobody's removed from it, and it isn't disbanded, and
  so a character in it can't be deleted: "Nobody leaves a guild at war until the war's over." Each
  checks in its transaction, after the guild's row is locked, which a war's yes locks too, so a war
  can't start between the check and the change (`NotAtWar` in `server/guilds.cc`).
- The worlds count the kills they hand the saver, `nanommo_war_kills_total`, which the scores in
  MySQL should add up to, and `war-replay` checks.

## Alternatives considered

- Keeping the counts, and deleting a batch's kills from the stream in the same step as MySQL's
  commit. Redis and MySQL can't commit together, so there's always a moment between the two where a
  failure leaves one done and the other not. A row with an id makes a second time harmless, which
  is what the trades do.
- The stream entry's id as the kill's. It's unique within one stream in one Redis, made from
  Redis's clock, which does as long as there's one Redis whose clock never goes back. The world
  server's id is the kill's own, as a trade's is, whichever Redis takes it.
- Checking the five minutes in the saver, in SQL against `war_kills`, as it inserts each kill. That
  holds across instances and handovers, but it's a statement and a lookup for each kill in each
  batch, in place of one insert for all of them, and rows for kills that don't score, or a column to
  say so. In the world server's memory it's a map and three lines, and what it gets wrong is small
  (below).
- Assists, a level floor for a kill to score, protection for a player who's just come back, and
  alerts on the same two players killing each other again and again (not weighed in an episode).
  Each is more rule, and the five minutes take the farming's score from 2.6 a minute to one kill in
  five minutes, which isn't worth running up.
- Letting a guild surrender, or leave a war at a cost (not weighed in an episode). A war of half an
  hour ends by itself, so waiting it out is short, and one more command is one more thing to refuse
  and test.
- Letting members leave, and counting the war as lost by a guild that's gone. The rows would still
  go with the guild, and a guild that's gone can't be shown its loss.

## Consequences

- A war's score matches its kills, whatever the saver reads twice: run again, `war-replay` counted
  294 kills, and the scores came to 294; 274, and 274; 275, and 275, with MySQL skipping 30 kills
  it had already (`WarKillsTest.KillsTheSaverReadsAgainScoreOnce`).
- Each kill is a row, which stays with its war, and goes with it when a guild that's not at war
  ends. A score is two counts on a key, read every five seconds.
- Farming scores one kill in five minutes for each victim: `farm` killed its partner seventeen times
  in six and a half minutes, and the score was 2 to 0 (`Wars.AVictimScoresOnceEveryFiveMinutes`).
  Two players can still trade one kill each way every five minutes.
- The memory of who scored is the instance's: the next instance, after a handover or a restart,
  starts with none, so a victim killed just before a deploy can score again just after. A deploy
  hands each world over once, so that's one more kill for each victim a deploy at most. Two
  instances during a handover each remember their own.
- A member who wants out of a guild at war, or a player who wants to delete their character, waits
  until the war's over, up to half an hour. The refusal says why.
- A guild of one that loses a war keeps its score on record: its leader can't leave, so can't end
  it, until the war's over.
- Rolling back to 0.91.0 leaves `guild_wars`' counts as they were before 0.92.0, so a war that
  went on through the rollback shows the score from then, and the kills since then aren't on it.
