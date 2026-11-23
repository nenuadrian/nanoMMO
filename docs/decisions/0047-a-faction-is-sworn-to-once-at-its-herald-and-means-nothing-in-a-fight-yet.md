# 47. A faction is sworn to once, at its herald, guilds have none, and it means nothing in a fight yet

## Status

Accepted

## Context

Until episode 97, a player belonged to nothing bigger than a guild, which is a handful of friends,
chosen and left at will ([0021](0021-guilds-in-mysql-transactions.md)), and the only fights
between players were duels and guild wars, which both sides say yes to
([0033](0033-players-fight-each-other-in-duels-that-end-at-one-health.md),
[0041](0041-guilds-at-war-hurt-each-other-anywhere.md)). The episodes after it bring a battle
zone, Campus Martius, where sides fight on a schedule, and those sides have to exist first, with
everyone on one. Episode 97 adds them: three factions, Concordia, Fortuna and Victoria. It had to
settle when a player chooses one, whether they can change it, how a faction relates to a guild,
what a faction does before the zone exists, and where a faction is kept.

## Decision

- A faction is content: `content/factions.txtpb`, with its schema, `factions.proto`, has each
  faction's `id`, `name`, `color` and `herald`, a kind of NPC. `CheckContent` refuses a faction
  whose herald isn't in the NPCs file, and the content graph has a faction its herald `ENLISTS`
  players in, so one whose herald stands nowhere is out of reach. The factions are in
  `ClientHash`.
- A new character is in none. A player swears to one at its herald, in the Meadows: a click opens
  the herald's window (`client/herald_window.h`), Enlist asks once more, as it's for good, and Swear
  sends `Input`'s `enlist`, the herald's id. `World::Enlist` checks it as it does a trainer: an NPC
  in the player's land, within talking reach (`kTalkReach`), who's a herald.
- It's for good, for now. A player who's sworn is refused, "You're sworn to $0 already.", whichever
  herald they ask, and nothing in the game takes a faction away.
- A player's faction is part of what the world saves of them (`Progress`), into Redis with the
  tick's save, and the saver takes it to `players.faction`, from V18, as it takes their skills, with
  `COALESCE(players.faction, saved.faction)`: it writes one only where the row has none.
- A faction goes to everyone near a player in `Player`'s `faction`, with their name, once a second,
  and the client draws a diamond of its colour before their name, and its name under their own
  frame.
- Guilds have no faction. Guildmates can be in different factions, and a guild war is between two
  guilds, whatever their players' factions.
- A faction means nothing in a fight yet: `World::Hostile` doesn't read it, so its players can't
  hurt another faction's, or be hurt by them, outside a duel or a guild war, as before.

## Alternatives considered

- Choosing a faction as the character's made, on the screen with the looks
  ([0044](0044-looks-are-chosen-once-and-names-show-over-whoever-is-pointed-at-or-picked.md)). A
  new player knows nothing of the world yet, and the login server, which makes characters, reads no
  content. A herald in the world is a choice made in the world, by a player who's seen it, and it
  goes through the world's rules and its saves, which the offline world runs too.
- Leaving a faction, or swearing to another (left out of episode 97). Sides that a battle zone
  counts on would empty and refill between battles, and changing needs a rule for how often and at
  what cost, which can come once the zone shows what a change should cost. Refusing for good is
  the simplest rule that can be loosened later.
- Factions of guilds, where a guild swears and its members follow. A guild is a few friends, and
  friends can want different sides; and a player leaving or joining a guild would change their
  faction, which is meant to be for good. A war between guilds stays between guilds.
- Factions hostile to each other everywhere, as soon as they exist. It's open player against
  player in every land, with what [decision 41](0041-guilds-at-war-hurt-each-other-anywhere.md)
  left out: rules to stop a player hunting the newest, who'd have joined a side on their first day.
  Campus Martius, in the episodes after this one, is where the sides fight, on a schedule, with
  everyone there come to fight.
- Keeping the faction in MySQL alone, written by the world server as the player swears, `UPDATE
  ... WHERE faction IS NULL`, as a guild's change is. It would be one statement in the tick, once a
  character, but a player's own state goes through Redis and the saver, so that a handover's next
  instance loads it from Redis, and the tick never waits on MySQL for one player
  ([0013](0013-save-players-through-redis.md)). The guard is the saver's `COALESCE` instead.
- A faction by its name on the wire, as a guild is. The content has the names, and the client
  translates content by its id, so it's one small number, and a sentence's arg is `Arg`'s
  `faction`.

## Consequences

- Every player is in no faction until they find a herald, and the heralds are in the Meadows
  alone, so a player who's gone on to the Ashlands without swearing swears on a trip back.
- Choosing wrong can't be undone, but by making another character. The herald's window says it's
  for good before Swear.
- A player saved in Redis with a faction, whose save the saver hasn't taken yet, keeps it as they
  enter, as Redis's is read when MySQL has none.
- A faction is a byte or two in each player's piece of a full world, once a second.
- A faction changes nothing in a fight, so for now it's a colour and a name, which a battle zone,
  or anything else that reads `faction`, can give a meaning later.
- The bots swear to each faction in turn, by their numbers, so a load test's crowd has all three.
- A new faction is another entry in the factions file, with its herald in the NPCs file, a new
  client for `ClientHash`, and its name in the translations.
