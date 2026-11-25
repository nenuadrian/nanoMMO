# 12. TCP for inputs and messages, UDP for the worlds

## Status

Accepted

## Context

Episode 3 had to pick how the client and the server talk. TCP delivers a stream reliably and in
order, and holds up everything behind a lost packet until it's sent again. UDP sends packets
without those guarantees, which suits positions, where the next one is newer anyway. Episode 3
started on TCP, and episode 9 measured both under loss, with netem dropping and delaying the
server's packets.

## Decision

- The client logs in and says hello over TCP, and its input, and anything that has to arrive, goes
  over TCP too, which means chat, bags, quests, events and guild news.
- The world goes over UDP once the client asks for it, by sending its token again in a datagram to
  the world's port, four times a second until worlds come back that way. The server sends them to
  the address the datagram came from, which home routers let through, as the client spoke first.
  Until then, or if UDP can't get through, the worlds keep coming over TCP.
- Over UDP, the world goes out in parts of up to 1,200 bytes, so each fits in a packet and IP
  never splits it into fragments, which would all be lost with any one of them
  (`protocol/parts.h`). Each part carries the player's own fields.
- The client and the bots take their worlds over UDP by default, and over TCP with `tcp` after the
  host.

## Alternatives considered

- TCP for everything, as from episode 3. In the kind of MMO we're building, you pick a target and
  fight it, so a hiccup now and then ruins nothing, and World of Warcraft ran on TCP for years.
  Episode 9 measured it. Over TCP, the p99 of a world's age climbs with each step of loss, and at
  ten percent TCP can't keep up with 200 clients at all, while over UDP it stays within a tick or
  two of no loss ([Networking](../networking.md)).
- UDP for everything, with our own code to make what matters arrive, in order. That's a fair bit
  of work, and there are whole libraries for it (episode 3).

## Consequences

- Two transports to keep, and the world split into parts and put back together. A lost part leaves
  its players and monsters where they were for a tick, which episode 69 smooths over
  ([0023](0023-draw-everyone-else-in-the-past.md)).
- Over UDP, an event can come before the world with its unit, so the client waits for the unit
  before it draws the number (episode 49).
- TLS needs TCP's stream, so the world over UDP is plain
  ([0014](0014-tls-with-an-authority-of-our-own.md)).
- Leaving out what hasn't changed needs to know which world a client has whole, every part of it
  ([0024](0024-leave-out-what-has-not-changed.md)).
- Each world publishes its port for TCP and UDP both, in Compose and on a server, and its next
  instance's as well ([0016](0016-deploy-with-compose-to-one-server.md)).
