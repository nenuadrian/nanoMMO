# 17. Send each player what's near them, by cells

## Status

Accepted

## Context

Until episode 20, each tick, the server sent each player the whole world: every player and every
monster. With N players, that's N worlds of N players each, which grows with the square of the
players, and most of it was hidden in the fog on the client anyway. Episode 20 also sent the bots
on trips across the island, so they'd spread out as players would.

## Decision

- `server/interest.h` sorts the players and the monsters into cells of 32 by 32 metres each tick,
  and the NPCs once, as they stand still. A player gets their own cell and two cells each way
  around it (`kInterestCells`), so everyone within 64 metres, and some further out. This is called
  interest management.
- Each player and monster is serialized once a tick, into a piece, and a cell's world is its
  pieces one after another ([0003](0003-protobuf-as-the-contract.md)).
- Names, levels and the most health a unit can have go out with every twentieth world, once a
  second (`kFullEvery`), and the client keeps the last ones in between.

## Alternatives considered

- The whole world, as before. With bots spread over the island, on an Apple M4 Pro, it kept 20
  ticks a second up to 800 players, and sent each of 500 players 377 KB a second
  ([Networking](../networking.md)).
- Measuring the distance between each pair of players, which grows with the square of the players
  again, where finding a cell's neighbours is a lookup (episode 20).
- Three cells each way, everyone within 96 metres, which is where episode 20 started: 20 ticks a
  second up to 1,000 players, against 1,200 within 64 metres.

## Consequences

- Within 64 metres, with names and levels once a second, a tick took 26 ms (p50) at 1,200 players,
  and each of 500 players got 94 KB a second. On a VPS with 2 vCPUs, the server kept its 20 ticks
  up to about 700 players, where before it fell behind past 400.
- A player who comes into view does it past the nameplates, which show within 50 metres, and partly
  in the fog.
- The players in a cell shared one world, and over UDP its datagrams, until each client's world
  became its own in episode 70 ([0024](0024-leave-out-what-has-not-changed.md)).
- What happens near a player, the events, goes by the same cells (episode 49).
- How far a player sees is one constant, and so is the cell's size.
