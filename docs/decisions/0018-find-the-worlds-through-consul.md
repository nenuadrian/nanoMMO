# 18. Find the worlds through Consul

## Status

Accepted

## Context

Episode 21 gave the game more than one world, each a game server with players of its own. The
client had to find the worlds, and the login server list them, including after one crashed or a
new one started.

## Decision

- Each world registers with Consul, HashiCorp's service registry, and gives Consul its name, its
  address, the host and port players reach it at, its metrics port, how many players it has, when
  it started, and, from episode 27, the hash of its content ([0019](0019-content-as-data.md)).
- Its check is a TTL. Every two seconds, the tick loop sends a heartbeat, which a thread of its own
  takes to Consul, and ten seconds without one turns the world critical, so it drops out of the
  list. A world that stops deregisters, and Consul forgets one that stays critical for a minute.
- The login server asks Consul for the worlds whose checks pass, and Prometheus finds what to
  scrape there too ([0009](0009-prometheus-grafana-and-loki.md)).
- From episode 36, each instance of a world registers under its name and port, such as
  `Ember-7777`, and the login server lists each world at its newest instance.
- Consul runs in development mode, as one server, which keeps everything in memory.

## Alternatives considered

- A list of worlds in the client's configuration, which means a new client for each new world, and
  a list that's wrong as soon as a world goes down (episode 21).
- A table in MySQL that each world writes its address into. A world that crashes leaves its row
  behind, and players are sent to a server that's gone (episode 21).
- DNS, which can give several addresses for a name, and says nothing of a world's health or its
  players (episode 21).
- etcd or ZooKeeper, other registries, or Kubernetes, which does this for its own containers
  (episode 21).

## Consequences

- A new world needs no change to the client, the login server or Prometheus. A third one starts
  with one command ([Worlds and lands](../worlds.md)).
- The heartbeat comes from the tick loop, so a world whose tick is stuck drops out too.
- Consul forgets its worlds when it restarts, so each registers again within two seconds, and
  after any error. In production, Consul would run as three or five servers, so that one can fail.
- A world that leaves Consul leaves Prometheus's targets with it, which `WorldDown` had to allow for
  (episode 30).
- cpp-httplib's requests block, so they stay off the tick's thread
  ([0008](0008-one-thread-for-the-world.md)), and inside Compose's network they're plain HTTP.
- A world registers the address it reaches Consul from, and players reach it at its public host,
  so a world left at `localhost` can't be played from another machine. From 0.65.0, reaching one
  from another machine says to set `NANOMMO_PUBLIC_HOST`.
