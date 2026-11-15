# 22. Numbered inputs, and client-side prediction

## Status

Accepted

## Context

The server decides where everyone is ([0002](0002-an-authoritative-server-at-a-fixed-tick.md)),
so until episode 68, a key moved you a round trip later. At 150 milliseconds, you started and
stopped walking a beat after your fingers. Your own keys are what you feel most, and most games
hide that delay for your own player.

## Decision

- The client sends its keys once each of the server's ticks, 20 a second, each numbered from 1
  (`Input.sequence`), where it used to send them when they changed.
- The server applies a player's numbered inputs in order, one a tick, before the world steps. It
  keeps up to five waiting, and drops the oldest past that. With more than three waiting, it's
  fallen behind the client, and skips one a tick until it's caught up, and if two or more waited
  at the start of every tick for a second, it drops one, as inputs come in as fast as they go out.
- Each world says the last of your inputs the server applied (`acked`), and whether it moved you
  itself that tick (`steered`), with where you are, in every datagram.
- The client walks you with each tick's keys as it sends them, with `WalkAlong`, from the library
  the server walks players with, `nanommo_walking`. Each world puts you where the server has you,
  and walks you again through the inputs it hasn't applied (`client/prediction.h`). When the two
  disagree, the server wins, and the correction fades over a tenth of a second, unless it's a jump
  of two metres or more, as through a gate.
- While the server moves you itself, along a walk you asked for, up to a monster, with a skill, or
  as you die, the client draws you as the server says, and your keys don't walk you.

## Alternatives considered

- Waiting for the server, as before.
- Predicting everything, what the server steers too, which `ep068/step-3` did. While the server
  moved you along a walk, up to a monster or with a skill's stride, the keys walked you as well,
  and each world pulled you back, so step 4 leaves those to the server.

## Consequences

- A step has to come out the same on both sides, to the centimetre, or each world would pull you
  back a little. So walking is one library that both link, built without fusing a multiply and an
  add, like the terrain, so a Mac and Linux round it alike.
- Each client sends 20 inputs a second. The bots did too from episode 70, so that they say which
  world they have often enough ([0024](0024-leave-out-what-has-not-changed.md)).
- An input waiting in the server's queue is a tick later for each one ahead of it, which is why a
  queue that stays full is drained.
- The round trip is measured, from sending an input to the world that says it was applied, and
  shows under the minimap.
- You're drawn now, where the prediction has you. Everyone else is drawn a moment in the past
  ([0023](0023-draw-everyone-else-in-the-past.md)).
- A bot's inputs aren't numbered, and go in at once, as before.
