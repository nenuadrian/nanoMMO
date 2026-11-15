# 23. Draw everyone else 100 milliseconds in the past

## Status

Accepted

## Context

The server sends a world 20 times a second, evenly, and they don't arrive evenly: one can be late,
two can come together, and over UDP one can be lost
([0012](0012-tcp-for-inputs-udp-for-worlds.md)). Until episode 69, each figure eased towards the
newest place the server had sent, so a world that came late, or not at all, showed as a stop and
then a lurch. Episode 68 had looked after your own player, and nobody else
([0022](0022-numbered-inputs-and-client-side-prediction.md)).

## Decision

- The client keeps the last second of worlds, each with the time the server sent it, on the
  server's clock, and the time it came, on the client's (`client/interpolation.h`). A world's other
  parts fill it in as they come.
- The two clocks needn't agree. The world that came quickest says best how far apart they are, as
  the others were held up on the way by that much more.
- It draws everyone else as they were two ticks ago, 100 milliseconds (`kDelay`), on the server's
  clock, between the two worlds on either side of then. Two ticks is enough for a world up to a
  tick late.
- With no world after then, as when the next one is lost, the last two carry everyone on the way
  they were going, for two ticks at most, and then they wait for it.
- You're the prediction's, drawn now.

## Alternatives considered

- Easing each figure towards the newest world, as before, which turned every late or lost world
  into a stop and a lurch.
- Carrying everyone on from the newest world alone, which is the obvious alternative, called
  extrapolation or dead reckoning, and wasn't weighed. Here it only bridges a lost world, two ticks
  at most.

## Consequences

- Everyone else is a tenth of a second behind where the server has them, and you aren't. Someone a
  tenth of a second behind is hard to notice, where a stutter jumps out, and that's the price of
  seeing them move smoothly (episode 69).
- A lost world no longer shows: for up to two ticks, the others walk on, and past that they stop
  and wait, rather than jump.
- The animator takes the places as they come, without easing behind them, as the interpolation has
  smoothed them already.
- The client learns the server's clock from the worlds themselves, so the player's machine needn't
  share a clock with the server, as the bots must to measure a world's age
  ([Monitoring](../monitoring.md)).
