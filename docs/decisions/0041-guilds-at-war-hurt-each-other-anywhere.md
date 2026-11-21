# 41. Guilds at war hurt each other anywhere, for half an hour, and count their kills

## Status

Accepted

## Context

Until episode 91, a player hurt another only in a duel that both had said yes to, which ends at
one health and gives nothing ([0033](0033-players-fight-each-other-in-duels-that-end-at-one-health.md)).
That decision left players hurting each other anywhere, with the rules that go with it, for a
later episode: who may attack whom, what a death costs, and what stops a player hunting the
newest.

Episode 91 brings it in through guilds, which are in MySQL, each change a transaction
([0021](0021-guilds-in-mysql-transactions.md)): a guild is a group that's chosen to be one, with a
leader to speak for it, and a war between two is something both can say yes to. It had to settle
who may hurt whom, what a kill does, where a war is kept, as the world's library knows nothing of
MySQL ([0008](0008-one-thread-for-the-world.md), [0030](0030-offline-the-client-runs-the-worlds-rules.md)),
how a war started on one instance reaches the other during a handover, and how a war's kills are
counted.

## Decision

- A guild's leader asks another guild in the world to war, with `/war` and its name. The ask is a
  key in Redis, `war_ask:<world>:<guild>:<enemy>`, set with `SET ... NX EX 60`, as an invitation
  is. The other guild's leader asking back is the yes, as in a duel.
- A war is a row in `guild_wars`, from V14, with both guilds, when it started, when it ends, half an
  hour on (`Guilds::kWarLength`), and how many of the other side's players each side has killed.
  A guild is in one war at a time. The yes is a transaction that locks both guilds' rows, the lower
  id first, before the leader's own, and checks that neither is at war (`Guilds::StartWar`), so a
  change to a guild still locks its row before any member's, in one order.
- A war ends when its time is up. There's no surrender.
- Members of the two guilds can hurt each other anywhere, in any land, without asking, and nobody
  else can hurt a player, but a duel opponent. The server tells each World each player's guild and
  the world's wars (`World::SetGuild`, `World::SetWars`), and `World::Hostile` says who may hurt
  whom. `World::Pick` keeps `target_player` for anyone hostile, and `World::Land` hurts each hostile
  player a blow reaches, where the attacker saw them, up to 200 milliseconds back
  ([0034](0034-duel-blows-are-checked-where-the-attacker-saw-the-target.md)), without pushing or
  throwing them.
- A war's blow kills, as a monster's does, with the ten seconds' wait, and a kill gives no
  experience and drops nothing. It's the killing blow's, with no assists.
- The server reads the world's wars from MySQL every five seconds (`kReadWarsEvery`), in the tick,
  as it reads a roster, so that a war accepted on the other instance during a handover, or one
  whose time is up, takes effect.
- Each kill goes into Redis, in a stream of its own, `war_kills`, in the same fenced script as the
  tick's save (`Players::Save`), as a trade's record goes into `trades`. The saver adds each batch's
  kills to their wars' counts, in the batch's transaction, and then takes them out of the stream
  (`Players::Flush`).

## Alternatives considered

- Players hurting anyone, anywhere, or in some lands, as decision 33 put it (not weighed in an
  episode). It needs rules of its own to stop a player hunting the newest: levels, safe places, a
  cost to killing. A war takes two guilds that both said yes.
- A war's blows ending at one health, as a duel's do. Nobody would die, so a guild would have
  nothing to win, and a fight between two crowds would go on until one side walked off.
- A war until one side gives up, or a surrender (not weighed in an episode). A war that one side
  never ends goes on forever, and a surrender is one more command to refuse and test, where a war
  of half an hour ends by itself.
- Telling the other instance of a new war at once, through Redis's publish and subscribe, as well
  as reading MySQL (not weighed in an episode). It's another channel to keep up, for a war that
  starts on the other instance only while the players move to it, and five seconds' delay then is
  as good.
- Keeping a world's wars in each World, or in Redis. Two instances of a world run at once during a
  handover, and a war changes two guilds, which are in MySQL
  ([0021](0021-guilds-in-mysql-transactions.md)), whose row locks keep a guild in one war at a time
  on either instance.
- Counting the kills in MySQL as they happen, from the world server, in the tick. A statement in
  the tick for each kill would make it wait on MySQL in the middle of a fight
  ([0008](0008-one-thread-for-the-world.md)), where the saver's batches already go there.

## Consequences

- A player at war can die to an enemy anywhere, in the middle of a fight with monsters, and a
  duellist an enemy kills loses the duel, as one a monster kills does
  (`Wars.ADuellistAnEnemyKillsLosesTheDuel`).
- Nobody outside a war or a duel can hurt a player, nor a guildmate, even with a skill that hits all
  around (`Wars.NobodyElseCanHurtAPlayer`).
- A war starts at once on the instance where it's accepted, and on the other within five seconds.
  It ends up to five seconds after its half hour, and if MySQL can't be read, once it can.
- A death in a war is a death in the analytics, with no monster.
- The score is a few seconds behind the game, as the roster's levels are: up to five seconds for
  the saver's flush, and up to five more for the world to read it. A kill whose save Redis doesn't
  take, as when it can't be reached, isn't counted.
- A blow by a player in a war or a duel goes through each player in their copy of the land. Most
  blows are by players in neither, which pass the players by.
- The bots pass over a guild's news, so they take no part in wars.
