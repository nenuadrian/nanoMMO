# 2. An authoritative server, at a fixed tick

## Status

Accepted

## Context

Episode 1's architecture leans towards a thin client, where the server does the simulation, and the
client takes the input and draws, as we can't trust a client running on someone else's machine. In
episode 2 the client still moved its own cube. Episode 3 gave the game its server, and had to
decide which side says where a player is.

## Decision

- The world server decides what happens in the world, and the clients show it. A client sends its
  input, `Input` in `protocol/nanommo.proto`. That's the direction its keys give, whether it runs or
  swings, a skill, a monster it picked, a place it asks to walk to. It never says where it is.
- The world steps at a fixed tick, 20 a second (`TickInterval(20)`), on a steady timer, whose clock
  doesn't jump when the time of day is changed. After each tick, the server sends each client the
  world.
- The server checks what it's sent. A direction is scaled back to a length of one, and ignored if
  it isn't a finite number (episode 3); what players say is cleaned, and limited, on the server
  (episode 23).

## Alternatives considered

- The client decides where its player stands, as in episode 2. A modified client could then put
  its player anywhere, and the other clients would have no way to tell (episode 3).
- Reacting to each packet the moment it arrives, where an MMO server steps the world in fixed
  ticks (episode 1).
- A database that runs the server's logic, as SpacetimeDB does for Elegon, which episode 1 points
  to as quite a different take from ours.

## Consequences

- A key moves you once your input has reached the server and the world has come back. That's a few
  milliseconds on one machine, a beat behind your fingers over the internet. Episode 68 hides it
  for your own player ([0022](0022-numbered-inputs-and-client-side-prediction.md)), and episode 69
  smooths everyone else ([0023](0023-draw-everyone-else-in-the-past.md)).
- A tick has 50 milliseconds for the whole world, which decided where slow work goes, with logins
  ([0011](0011-a-login-server-of-its-own.md)), saving ([0013](0013-save-players-through-redis.md))
  and the rest in [0008](0008-one-thread-for-the-world.md).
- What decides a fight stays on the server. The content's hash for the clients leaves out the
  fights and the skills' effects, so a change to a sword's damage needs the worlds restarted, and
  no new client ([0019](0019-content-as-data.md)).
- `world_fuzzer` plays the game with whatever a client might send, and checks that each player
  stays somewhere real (episode 28).
- To show a move at once, the client runs some of the server's rules itself, the bag's and
  walking's, and the server's answer still wins (0.64.0, 0.68.0).
