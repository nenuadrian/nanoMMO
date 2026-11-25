# 19. Content as data, in protobuf's text format

## Status

Accepted

## Context

In episode 26, the sword lived in two places. The server had its reach and its timing in code,
and the client its model and its grip in code of its own. Episode 27 found the monsters and the
NPCs the same, and more content was coming, with items, skills, quests and, later, people.

## Decision

- The content is data in `content/`, which the world servers, the client and the bots read with
  one library, `nanommo_content`. Each kind has a `.txtpb` file, in protobuf's text format,
  with its schema next to it, in a `.proto`, and two header lines that name the schema for
  editors.
- Each side reads what it needs. A weapon's `weapon` and a monster's `fight` are the server's, and
  a `look` is the client's. An item is the parts it has, with no type, so a new item that puts
  parts we have together is another entry, and no code.
- The parser refuses a field the schema doesn't have, or a value of the wrong type, with its file
  and line. Then the library checks what types can't, in each file and across them, and the
  servers, the client and the bots don't start without good content. A test reads the real folder,
  so the Docker images, which copy from the test stage, don't build with bad content either.
- Ids start at one, and an item or a kind keeps its id for good, as they're saved and sent by id.
  `retired` lists the ids that are gone, which no item can take again.
- Each world registers a hash of what the client reads with Consul, and a client stays off a world
  whose content isn't its own (episode 27).

## Alternatives considered

- A C++ table, which the compiler checks, where a change to a sword's damage means building and
  shipping the server and the client again (episode 26).
- JSON or YAML, which are easy to edit, and have no schema our C++ knows, so we'd read each field
  by hand, and a misspelt one would quietly fall back to its default (episode 26).
- A table in MySQL, as TrinityCore keeps its items, which suits the server, while the client
  still needs a copy of its own, which is two places again (episode 26).

## Consequences

- A change to the sword's reach is an edit and a restart of the worlds, and a new client only when
  it changes what the client reads. The fights and the skills' effects stay out of the hash, so
  tuning them needs no new client.
- The server's image has to carry the files. `ep026/step-2` read the items file in an image that
  didn't have it, and the server stopped in Compose. The image copies `content/*.txtpb` since.
- Monsters and NPCs go to the clients by kind, and the fields that carried their names and looks
  are reserved in the protocol (episode 27).
- The people moved in too, as data, with figures (episode 44) and a rig of our own (episode 60),
  whose motions the client works out as it loads. They replaced Mixamo's files, whose terms kept
  them out of git, so a fresh clone draws everyone as a person, never as a cube.
- The text format costs no new dependency, as protobuf has been in the build since episode 3.
