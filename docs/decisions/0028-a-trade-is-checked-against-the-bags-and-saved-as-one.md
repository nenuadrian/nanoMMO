# 28. A trade is checked against the bags, and saved as one

## Status

Accepted

## Context

Episode 74 added trading in a simple version (`server/trades.h`). An offer kept a copy of each
stack offered, and the trade gave those copies, and took as many off each place, ignoring a place
that was empty by then. A ready was for whatever was offered when it arrived. Each bag was then
saved with its own player, after the next tick ([0013](0013-save-players-through-redis.md)).

In episode 75 the red-team client ([0025](0025-the-server-checks-what-a-client-asks-for.md)) found
two ways to keep a sword it gave, each three swords from two.

- `swap` offered its sword from the bag, and once its partner was ready, moved the sword to
  another slot and was ready too. The partner got the copy, and the sword stayed where it went.
- `run` offered its sword, and once its partner was ready, was ready too and closed its
  connection at once. The trade happens as the ready arrives, in the handler of its message, and
  nothing saved a player who'd gone, so its bag in Redis was the one from before the trade, while
  the partner's was saved with the sword at the next tick.

Both trusted something the server can check, that the places still hold what was offered, and
that both bags of a trade are saved, or neither.

## Decision

- A place a player offers from is held until the trade's over. `World::MoveItem`, `UseItem` and
  `DestroyItem` won't move, use, wear or throw away what's there, or put anything there
  (`Trades::Holds`), and say "You can't, while it's offered in your trade."
- Before it gives anything, `Trades::Complete` checks that each place offered has the same item,
  with the same rolls, and at least as many as were offered, in copies of both bags, so that it's
  all of it or none. At least, rather than exactly, as a kill can add to a stack that's offered, and
  what it adds stays the player's.
- A trade has a revision, `Trade.revision`, which each change to either offer takes to the next.
  A ready carries the revision its client saw, `TradeAction.revision`, and a ready for an earlier
  one does nothing but say "The trade has changed, so look at it again."
- A trade that's done saves both players there and then, before either hears of it, and
  `Server::Leave` saves a player as they go, except while the world hands its players over, as the
  next instance has loaded them already.
- `Players::Save` is one Redis transaction, `MULTI`, the commands and `EXEC`, in one round trip
  (`Redis::Transaction`), for the tick's save too. It fails if Redis couldn't queue a command, when
  it runs none of them, or if `EXEC`'s replies have an error in them.

## Alternatives considered

- Checking the bags as the trade completes, without holding the places. `swap` would get nothing,
  but an honest player who moved what they'd offered would see the trade fail later, for a reason
  they couldn't see. Holding the places says why at the move, and the check stays, for whatever
  else changes a bag.
- Taking what's offered out of the bag as it's offered, into the trade, and giving it back if the
  trade's called off (the obvious alternative, not weighed in an episode). Nothing could move it,
  but every way a trade ends, a cancel, a leave, a gate or a handover, would have to give it back,
  and a crash in between would lose it.
- Saving the two bags in one pipeline, as the tick did. A pipeline is one round trip, but Redis
  runs each command on its own, so a connection lost half way saves one bag and not the other.
- Saving a trade in MySQL, as one transaction, as guilds are
  ([0021](0021-guilds-in-mysql-transactions.md)). The world's thread would wait for MySQL at each
  trade, and the bags would be newer in MySQL than in Redis, which a player is loaded from first.

## Consequences

- A trade is a Redis round trip in the handler of the second ready, on the world's thread, as a
  guild change is a MySQL one. Trades are on the bag's limit, ten at once and then ten a second.
- Every save is a transaction. Redis runs its commands with nothing in between, so other clients
  wait for it, as they mostly did for the pipeline, whose commands arrive together.
- Redis doesn't undo a transaction's other commands when one fails as it runs, say on a key that
  holds another type. `Redis::Transaction` reports it, and the next tick saves the players again.
  If the save as a trade's done fails, the trade has happened in memory, and the next tick or the
  leave saves it. If Redis is down for all of those, a player who leaves meanwhile loses what
  changed since they were last saved, as before.
- The saver takes the bags to MySQL a batch at a time, so MySQL can have one side of a trade a
  flush before the other. Redis has both, and a player is loaded from Redis first.
- An honest player who moves what they've offered is told why, and takes it out of the offer
  first. A ready that crossed a change to an offer on its way has to be pressed again.
- What's still trusted is that two stacks of the same item, rolls and count are the same thing, so
  a copy can't be told from the one it was made from. Serials on items, a lease on each player so
  that one server saves them at a time, and an audit trail of trades are for a later episode.
