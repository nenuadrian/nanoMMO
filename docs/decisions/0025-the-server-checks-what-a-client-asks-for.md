# 25. The server checks what a client asks for, and a red-team client tries it

## Status

Accepted

## Context

Since episode 3 the server decides what happens in the world, and a client only asks
([0002](0002-an-authoritative-server-at-a-fixed-tick.md)). The server checked some of what it was
sent, a direction's length and whether it's a number, but nobody had tried each of the client's
rules from the other side. In episode 72, a client that breaks them one at a time found that putting
the weapon in the bag and back, as each blow landed, started the next swing as soon as the weapon
was back, about twice as often. That had been possible since episode 40.

## Decision

- Everything a client sends is taken as made up. What it asks for is checked on the server, against
  the world as the server has it, never against anything the client says about itself.
- `nanommo_cheat` is the server's red-team client, in the repo with the game. It tries each rule a
  client should keep and prints what it gets for breaking it. A server test proves each rule holds.
- A cheat found to work gets its fix and its test in the same release, so the red-team client never
  goes public ahead of the fix for what it finds.

## Alternatives considered

- Anti-cheat in the client, as many games ship (the obvious industry alternative, not weighed in an
  episode). It runs on the player's machine, which the player controls, so it can be patched out,
  and it has to watch the player's computer to work.
- Trusting the client, and finding cheaters afterwards, from the logs and the analytics. That finds
  them once the damage is done, and a server that checks still gives those numbers.

## Consequences

- Each new kind of input needs its check on the server, and a way for the red-team client to try it.
- The red-team client is public, with the fixes for what it found.
- A check the client also does, such as a cooldown it shows, is done twice: once to show it, and
  once, on the server, to keep it.
