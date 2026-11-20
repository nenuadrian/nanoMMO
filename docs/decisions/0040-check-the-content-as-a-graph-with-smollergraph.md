# 40. Check the content as a graph, with smollerGraph, in the tests

## Status

Accepted

## Context

The content is links from one thing to another: a quest to the NPC who gives it and the monster it
asks for, a monster to what it drops, an NPC to the land it stands in, a land to the one behind its
gate. `CheckContent` checks that each link has something at its other end. Nothing checked that a
player can ever get to each thing. Taking the Guard out of the Meadows, or a monster's camps out of
its land, or the Leather Cap out of the Skeleton's loot, passes `CheckContent` and the content
tests, and the game starts: a quest nobody can take, a quest nobody can finish, an item nobody can
get. Whether something is reachable from a new player is a question about paths, which is what a
graph database answers, and smollerGraph, from its own series, became a library another project can
fetch in its episode 20.

## Decision

- `content/graph.h` adds the content to a graph in smollerGraph: a node for each land, kind of
  monster and NPC, quest, item and skill, and a new player, and a relationship for each way a
  player gets from one to the next. A quest asks for its monster, which isn't a way to get there.
- `Unreachable` asks for a shortest path from a new player to each node, in Cypher, along any of the
  ways, and lists the nodes without one, and the quests whose monster is out of reach.
- The tests check the game's content has nothing out of reach, and that each kind of mistake is
  found, so CI checks each change to the content. `nanommo_graph` writes the graph to a folder for
  smollerGraph's `sg` to serve, with its explorer.
- smollerGraph comes in with FetchContent at a version, v0.20.0, with its SHA-256, and builds with
  our Abseil. The world servers, the client and the bots don't link it.

## Alternatives considered

- A walk of our own over the content, in C++: a breadth-first search over a dozen kinds of link,
  written once, and a few hundred lines to keep. smollerGraph has the search, and its explorer
  shows the graph, and the checks read as queries.
- The check in `ReadContent`, as the servers start: a world would refuse content with something out
  of reach, and the game would link a graph database it doesn't otherwise need. A test stops the
  content before it's merged, which is where a mistake costs least.
- Neo4j, in Docker, in CI: the database smollerGraph learns from, with more than we need, and a
  server to start in each test run.

## Consequences

- The tests depend on another project's releases, and a new version of smollerGraph is a change to
  the URL and its SHA-256, as with our other dependencies.
- Abseil is ours, which is newer than smollerGraph's, so a new Abseil has to build smollerGraph too.
- A new kind of link in the content needs a relationship in `content/graph.cc` too, or what's
  reached through it looks out of reach, and the tests say so.
