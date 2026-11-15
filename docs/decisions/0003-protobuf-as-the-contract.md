# 3. Protobuf as the contract between the client and the servers

## Status

Accepted

## Context

Episode 1 proposed Protocol Buffers for each message between the client and the server, and
episode 3 brought them in, with the first two messages: an `Input` from a client, and a `World`
from the server. Every message the game sends since goes the same way.

## Decision

- `protocol/nanommo.proto`, in proto3, in the package `nanommo.protocol`, is the contract between
  the client, the servers and the bots. The build runs protoc on it, and all sides compile the C++
  it generates, so neither writes its own encoding.
- Abseil and protobuf are fetched, pinned and built from source, protoc with them, so the
  generated code matches the runtime it's compiled against on any machine (episode 3).
- Over TCP, each message has its size in front, as four bytes in network byte order, and a size
  over 64 KiB is refused, so a broken or hostile stream can't make us buffer gigabytes
  (`protocol/framing.h`).

## Alternatives considered

- JSON. It's text, with each field's name in quotes: a message with one number field set to 150
  is nine bytes, where protobuf's is three, and the receiver has to parse the text back into a
  number (episode 1).
- A binary protocol of our own, small and fast, with the encoding and the decoding written by hand
  on both sides and kept in step by hand, where adding a field would break older clients
  (episode 1).
- Protobuf's editions, its newer way to choose its features. Most code and examples use proto3, and
  moving later should be a small change (episode 3).

## Consequences

- A field's number is what goes on the wire, so a field that goes is `reserved`, as a monster's
  name and an NPC's colour are since episode 27, and no new field takes its number.
- Code that doesn't know a field skips it. That isn't enough to let any client play on any server:
  a client of another version would log in, then fail to make sense of what the world sends it,
  so the login server lets in only the servers' own minor version
  ([0020](0020-one-version-for-the-whole-game.md)).
- Protobuf leaves out a field that's zero or empty. So content ids start at one, and names and
  levels go out once a second, while the client keeps the last ones (episode 20).
- Two messages one after the other read as one. So the server serializes each player and monster
  once a tick, into a piece, and a world is its pieces one after another (episodes 8 and 20), which
  leaving out what hasn't changed builds on ([0024](0024-leave-out-what-has-not-changed.md)).
- The content is in protobuf's text format, with its schemas next to it
  ([0019](0019-content-as-data.md)), and a bag goes into Redis as protobuf's bytes, and into MySQL
  as its JSON ([Saving players](../saving.md)).
- protobuf compiles with the rest, which makes a first build slow: the images take about fifteen
  minutes on a server with 2 vCPUs ([Deployment](../deployment.md)), and ccache keeps what it made
  from then on (episode 43).
