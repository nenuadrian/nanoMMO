# 24. Leave out of each world what hasn't changed, by its pieces' hashes

## Status

Accepted

## Context

Until episode 70, each world carried everyone near you, whole, 20 times a second, so a player
standing in a crowd cost the same as one walking through it. With forty bots on one machine, the
server sent each of them 16.8 KB a second, from `nanommo_bytes_sent_total`.

## Decision

- Each input says the newest world the client has whole, every part of it, by when it was sent
  (`Input.seen`). The bots say so too, and send their keys each tick for it, as the client does.
- The server remembers a second of what it sent each client, twenty worlds, with the copy of the
  land each was in, and a hash of each player's and monster's piece, the bytes it serialized for
  them (`Server::Delta`).
- A piece whose hash is the same in the world the client has, and in each world sent since, goes
  by its id alone, in `still_players` or `still_monsters`, and the client keeps it as it has it.
  The rest go whole, as before. This is called delta compression.
- A world the server no longer remembers, or from another copy of a land, whose ids are its own,
  is no help, and everything goes whole. So does a world to a client that says nothing.

## Alternatives considered

- Whole worlds, as before.
- Keeping each piece's bytes, where a hash is eight bytes. Keeping the bytes would mean a copy of
  each world for each client (episode 70).
- Comparing with the world the client has alone. The client may have had any of the worlds sent
  since, so a piece that changed and changed back would leave it with the change.
- Sending only the fields that changed in a piece, or compressing the world. These are the obvious
  alternatives, and neither was weighed.

## Consequences

- With forty bots on one machine, the server sends each 7.6 KB a second, from 16.8. It moves from
  run to run. The episode's own run went from about 20 to about 11.5.
- Those who move still go whole each tick, as a player walking changes where they are, and so
  their piece. What's saved is the ones standing about and the monsters resting.
- Each client's world is its own now, so the clients in a cell no longer share their datagrams,
  and the server puts a world together for each of them
  ([0017](0017-send-each-player-what-is-near-them.md)).
- The bots said which world they had only when they changed their mind, about once a second and up
  to three seconds apart, so the server had little to compare with until they sent their keys each
  tick (`ep070/step-3`).
- Two different pieces with one hash would be taken for the same. The hash is `std::hash` of the
  piece's bytes, 64 bits on our servers, and the next change sends the piece whole again.
