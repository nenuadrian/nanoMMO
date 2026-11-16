# 34. Duel blows are checked where the attacker saw the target, up to 200 milliseconds back

## Status

Accepted

## Context

A client draws everyone else 100 milliseconds in the past
([0023](0023-draw-everyone-else-in-the-past.md)), and its keys reach the server a round trip after
the world they answer left it. Until episode 82, the server checked every blow against where its
target was as the blow landed. Against a monster, which stands still to swing, nobody could tell.
Episode 81 brought duels ([0033](0033-players-fight-each-other-in-duels-that-end-at-one-health.md)),
where the target is a player, who runs as fast as the one swinging: at 3.1 metres a second, with a
round trip of 50 to 200 milliseconds, they were 0.5 to 0.9 metres from where the attacker saw them,
against a sword's reach of 1.6. A blow that looked in reach missed, and one that looked short could
land. `Duels.ASwingAtWhereTheyWereMissesWithoutCompensation` has one: bob 1.47 metres from alice
as her screen last had him, and 1.78 at the server as her blow lands.

## Decision

- Each World keeps where each of its players is after each step, for the last second
  (`server/history.h`), by a clock of its own, which goes on a step at a time.
- Each numbered input says when, on the server's clock, its client was drawing everyone else
  (`Input.drawn`, 21, from `Interpolation::Drawn`). As the input is applied, `Inputs` takes how far
  behind the server's clock that is, and the World keeps it for the player.
- A player's blows on their duel opponent are checked against where the opponent was that long
  before (`World::Seen`), and so are the aim and the run-up, which go by `World::Target`. Nobody is
  moved.
- The server believes 200 milliseconds at most (`Inputs::kMostRewind`): the 100 a client draws
  everyone in the past, and a round trip of up to 100. A time further back is taken as 200, and a
  time ahead of the server's clock as now, as is an input that doesn't say, as a bot's.
- Monsters are hit where they are.

## Alternatives considered

- Checking where the target is, as before, so that every blow on a moving player is a guess across
  the network.
- Believing whatever time a client says, back as far as the history goes. `stale`, the red-team
  client, says it drew its opponent a second before, and would hit them anywhere they'd been in
  that second, up to 3.1 metres behind them at a run.
- Going back by a round trip the server measures for each player, and the 100 milliseconds,
  without the client saying anything (not weighed in an episode). It's the trip as the server sees
  it, not the moment the client drew, and a client can still hold its inputs back to seem further
  away.
- A longer reach for a duel's blows while both players move, as some games give melee, instead of
  looking back (not weighed in an episode). It helps whichever way the target runs, so it helps a
  blow on someone coming closer, which needs no help.
- Going back for monsters too. A monster stands still to swing, and comes to whoever hurt it, so
  there's little to make up, and a blow pushes and throws it from where it is.

## Consequences

- A blow that looked in reach on the attacker's screen lands, up to 200 milliseconds back
  (`Duels.AHitOnWhereTheyWereLands`, `ServerTest.ADuelBlowLandsWhereTheClientDrewThemUpToTheMost`).
- The cost falls on the player who's hit: running off, out of reach on their own screen, they can
  still be hit up to 0.62 metres behind them at a run.
- Any client can say it's the most behind, so a modified one gets 200 milliseconds whatever its
  network: about 100 more than a client beside the server, 0.31 metres on a runner.
- A player whose round trip is over 100 milliseconds has to lead a runner a little, as everyone did
  before.
- Each world records a place a tick for each of its players, and each numbered input carries ten
  more bytes.
- The client draws its own swing as the server sends it, a round trip late, so on a slow network
  the blow shows on the attacker's screen after the moment it was checked at, with the opponent
  further on.
