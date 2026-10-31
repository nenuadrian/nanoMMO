# Worlds and lands

A world is a game server, `nanommo_server`, with players, monsters and lands of its own. Each one
registers with [Consul](https://developer.hashicorp.com/consul), HashiCorp's service registry, and
the login server asks Consul for the worlds that are up, so a new world needs no change to the
client, the login server or Prometheus. Compose runs two: Ember on port 7777, and Frost on 7787,
on an island from another seed. Consul's UI is at http://localhost:8500.

A world takes its port as its first argument (7777 by default), and the rest from settings:

| Setting | Default | What it is |
|---|---|---|
| `NANOMMO_WORLD` | `Ember` | Its name, up to 16 letters and digits |
| `NANOMMO_WORLD_SEED` | `1` | The seed its lands' islands grow from |
| `NANOMMO_PUBLIC_HOST` | `localhost` | Where players reach it, on its port; Compose takes it from your shell |
| `NANOMMO_METRICS_PORT` | `9464` | Where Prometheus reaches its metrics |
| `NANOMMO_CONSUL_HOST` | `127.0.0.1` | Where Consul is, on port 8500 |
| `NANOMMO_CONTENT` | `content` | The content folder (see [Content](content.md)); the image has its own |

- A world registers with its name, its address, the public host and port, the metrics port, how
  many players it has and when it started, and a health check that turns critical ten seconds
  after its last pass.
- Every two seconds, the tick loop sends a heartbeat, and a thread of its own takes it to Consul, so
  a slow Consul can't hold up a tick, and a heartbeat that finds the last one still waiting skips.
  Each heartbeat passes the world's check, so a world that stops ticking drops out of the list, and
  comes back when it ticks again. A world registers again when its players change, as registering
  again keeps the check's status, and after an error, as Consul forgets its worlds when it restarts.
  Consul forgets a world that stays critical for a minute.
- A world registers the address the kernel sends to Consul from, which is the address Consul and
  Prometheus reach it at, inside Compose's network. Players reach it at its public host instead,
  which in Docker is the machine the ports are published on.
- When it stops, a world deregisters, and leaves the list at once.
- A world's name is its ID in the `players` table, so two worlds with one name share their players.
  In Consul, each instance of a world registers under its name and port, such as `Ember-7777`, as a
  world can run as two instances for a moment, on two ports. The login server lists each world
  once, at its newest instance, and Prometheus labels each instance with its world, from the
  `world` in its registration.
- To play from another machine, start the worlds with the address of this one:
  `NANOMMO_PUBLIC_HOST=192.168.1.20 docker compose up -d`.
- Consul runs in development mode: one server, which keeps everything in memory. Each world
  registers again within two seconds, so a restart loses nothing for long. In production, Consul
  would run as three or five servers, so that one can fail.

A third world, with nothing changed in a file:

```bash
docker compose run -d --name gale -p 7797:7797 -p 7797:7797/udp \
  -e NANOMMO_WORLD=Gale -e NANOMMO_WORLD_SEED=3 server 7797
docker stop gale && docker rm gale
```

Each world keeps its own players: the `players` table has a row per account and world, and the
players in Redis are `player:<world>:<account>`. The players from before there were worlds stay in
Ember: the migration gives the table's `world` column Ember as its default, which also keeps the
servers from before saving while a deploy migrates, and the saver writes a player that was unsaved
before worlds to Ember. The rankings on the website show each player's world. The bots connect to
port 7777 of the host they're given, so they play in Ember (`server` in Compose).

## Lands

Each world has three lands, each an island of its own: the Meadows, where players start, the
Ashlands, where ash and charred trees stand around lava, under a red haze, and the Crypt, a dungeon
of grey stone around a green ooze. `terrain/lands.h` lists them for the server, the client and the
bots, in one order, as the server tells a client which land it's in by its place in the list, and
lists the gates between them:

| Land | Seed | Gates |
|---|---|---|
| Meadows | the world's seed | 10 metres north of the middle, to the Ashlands; 12 metres east, to the Crypt |
| Ashlands | the world's seed + 1000 | 10 metres north of the middle, to the Meadows |
| Crypt | the world's seed + 2000 | 10 metres north of the middle, to the Meadows |

The world server runs its lands as copies, in its one tick loop, each a `World` with its own players
and monsters (`server/copies.h`). Each open land has one copy, which everyone in the world shares. A
dungeon has one for each player who goes in, made as they go through its gate, with its own
monsters, so that nobody else is in it:

- A dungeon's copy that has nobody in it stops: its monsters stand where they are, and don't come
  back to life. It waits five minutes for its player, so that someone who steps out finds it as they
  left it, and then it goes.
- The players in a dungeon are saved outside it, three metres out of its gate in the land it leads
  to, as its copy may be gone by the time they're back. After a deploy it is, as it ran in the
  instance before: a player in a dungeon comes out of it, in the next instance, where they went in.
- A copy that has nobody in it gets nothing broadcast.
- The `nanommo_copies` metric counts each land's copies.

- Players wade into the Meadows' sea up to their knees, and not into the Ashlands' lava: each land
  says how deep its sea can be waded, and its `Terrain` takes that.
- A gate is a round platform, a rim of the land's stone with a disc in the colour of the sky of the
  land it leads to set into it, in a ring of six standing stones, each on a flat base and coming to a
  point. The client draws them as cubes in the shapes `client/cube.h` has, a box, a disc and a
  spire, each made of flat-shaded triangles.
  A player who walks within a metre of its middle goes to that land, and comes out three metres past
  the middle of its twin there, the gate that leads back, the way they were facing, so that they walk
  on out of it. Each gate has a twin, and each dungeon a gate, which a `static_assert` checks.
- The server sends each client its land and its land's seed, and the client builds that land in its
  look, from `client/atmosphere.h`, whenever the seed changes. That takes a moment, as it makes the
  island and its chunks again, so the client does it behind a loading screen (see
  [The loading screen](graphics.md#the-loading-screen)).
- The `players` table and the players' hashes in Redis keep the land a player is in, by its name, so
  they come back where they left. The migration gives the column the Meadows as its default, which is
  where the players from before were, and keeps the servers from before saving while a deploy
  migrates.
- The players-online metric has a `land` label, and the dashboard shows each world's lands apart.
- The bots go into one of their land's gates on one trip in five, so they spread over the lands, and
  each that goes into the Crypt gets a copy of its own.

Running a world's copies in one process makes a trip through a gate a move from one `World` to
another, within a tick. With a server for each land, each land could have a machine of its own, and
a trip would hand the player over from one server to the other, with a save and a token, like a
login.

## Walking by yourself

A double click on the ground, or a click on the map, walks you there by yourself, around what's in
the way. The client sends where (`Input.walk_to`), and the world finds the way and walks you along
it, a step each tick, at a run. The keys, a swing or a skill call the walk off, and so do dying and
going into another land.

- `terrain/paths.h` marks each metre square of a land a player can walk on: its middle, and four
  points 35 centimetres around it, are walkable, which keeps the way clear of the tree trunks.
  The world's copies of a land share its squares, made once, as the copies are made, in about ten
  milliseconds.
- A walk with nothing in the way goes straight, with no search. Otherwise, A* finds the shortest
  way over the squares: the squares to look at next wait in a priority queue, ordered by the cost
  so far plus the octile distance left, as far diagonally as it can and then straight. Each square
  has eight neighbours, and the way doesn't cut the corner between two squares that touch at it.
- A search looks at 40,000 squares at most, then goes as near as it got. On Ember's Meadows, a
  search from the middle to anywhere on the island takes about a quarter of a millisecond, and the
  slowest about two and a half, against a tick of fifty.
- The way is straightened to the places where it turns: from where you are, on to the farthest
  square still in sight, and again from there.
- The cost of each square and where it was reached from stay from one search to the next, and count
  only while stamped with the number of the search going on, so a search clears and allocates
  nothing.
