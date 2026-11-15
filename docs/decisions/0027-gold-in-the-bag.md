# 27. Gold in the bag's message, with its revision

## Status

Accepted

## Context

Episode 74 gave the game gold, which monsters drop, and trading, where two players swap items and
gold at once. A player's bag was already one message, `Inventory`, with a revision that goes up
with each change (episode 40). The server sends the bag when its revision moves, the saver writes
its rows to MySQL when it does, and `players.items_revision` (V8) keeps it. Everything else a
player has, where they are, their experience and health, goes into Redis with each tick that
changes it ([0013](0013-save-players-through-redis.md)).

## Decision

- Gold is a field of the bag's message, `Inventory.gold`, in no slot. Adding or taking it takes
  the bag to its next revision, like moving an item (`AddGold` and `TakeGold` in
  `server/inventory.h`).
- So it goes where the bag goes: to the player's client alone, over TCP, as the bag changes; into
  Redis in the bag's bytes; and to MySQL in `players.gold`, from V12, which the saver writes with
  `items_revision` when the bag has changed, and leaves as it is when it hasn't.
- A player carries a billion at most (`Inventory::kMostGold`), so that adding any two amounts never
  wraps a `uint32`. A monster's gold past that is lost, and a trade that would take a player past
  it doesn't happen.

## Alternatives considered

- Gold as an item, a stack of coins in the bag. It would take a slot, need a stack bigger than
  any other, and drag, split and throw away like an item, none of which gold should.
- Gold as a field of the player, beside their experience, saved each tick it changes. A trade
  changes items and gold together, and they'd change in two places, sent and saved apart.
- Gold in MySQL alone, each change a transaction, as guilds are
  ([0021](0021-guilds-in-mysql-transactions.md)). Each kill drops gold, so the world's tick would
  wait for MySQL at every kill.

## Consequences

- A change to a player's items and gold together, as a trade's, is one revision, sent and saved
  as one.
- Each kill now changes the bag, so each kill sends the player their whole bag and marks it for
  the saver, which writes its rows again. Most kills did already, for what they dropped.
- A trade changes two players, and each one's bag is saved with its own player, as any bag is.
