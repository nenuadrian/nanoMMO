# Networking

## Packet loss: TCP and UDP

The server sends each client the world over TCP, or over UDP once the client asks for it. To see what
a bad network does to each, `compose.yaml` has a `netem` service. It runs `bench/netem.sh` in the
server's network namespace, with the `NET_ADMIN` capability, and uses Linux's netem to delay the
server's packets and drop a share of them: a delay in milliseconds, then a loss in percent.

```bash
docker compose up -d --build
docker compose run --rm netem 50 5
docker compose run --rm netem 0 0
```

It turns off segmentation offload on the server's interface first. Otherwise the kernel hands netem
up to 64 KB at a time, to be cut into packets later, and netem drops all of those packets together.
It also lets netem hold 100,000 packets, as its default of 1,000 drops packets of its own once a busy
server has more than that waiting.

`bench/loss.sh` starts 200 bots over the transport you give it, then steps the loss from none to ten
percent, with 50 ms of delay, and after a minute at each step asks Prometheus for the p50 and p99 of
the bots' world age. Like `load.sh`, it needs `curl` and `jq`.

```bash
mkdir -p bench/results
bench/loss.sh tcp > bench/results/tcp.json
bench/loss.sh udp > bench/results/udp.json
```

Without loss, the two are level: a world is up to a tick old, plus the delay. Over TCP, the p99 climbs
with each step of loss, as the worlds wait for each lost packet to be sent again, and at ten percent
TCP can't keep up with 200 clients at all. Over UDP, the p99 stays within a tick or two of no loss. So
the client and the bots take their worlds over UDP by default.

How the world gets there over UDP:

- The client logs in and says hello over TCP, as before. Its input keeps going over TCP, and so does
  anything that has to arrive.
- It then sends its token again, in a UDP datagram, to port 7777, four times a second until worlds
  come back that way. The server sends that player's worlds to the address the datagram came from,
  which home routers let through, as the client spoke first. Until then, or if UDP can't get through,
  the worlds keep coming over TCP.
- The players and the monsters go out in parts of up to 1,200 bytes, so that each fits in a packet and
  IP doesn't split it into fragments, which would all be lost with any one of them. A lost part leaves
  its players and monsters where they were for a tick. `protocol/parts.h` splits the world and
  puts it back together. Each part ends with what's the player's own, their id, experience, seed,
  land, cooldowns and the skills they know, so the parts leave room for that, for the nine skills
  a hotbar holds.

## What each player gets

Each tick, the server sends each player the players and monsters near them, which is called interest
management. Sending everyone the whole world grows with the square of the players, and most of it
would be hidden in the fog anyway.

- `server/interest.h` sorts the players and monsters into cells of 32 by 32 metres each tick, and
  the NPCs once, as they stand still. A player gets their own cell and the two cells each way around
  it, so everyone within 64 metres, and some further out. A player who comes into view does that
  past the nameplates, which show within 50 metres, and partly in the fog. `kInterestCells` sets how
  far that is.
- The players in a cell get the same world, so the server puts it together once a cell. Each player
  and monster is serialized once a tick, into a piece, and a cell's world is its pieces, one after
  another, which protobuf reads as one message (`protocol/parts.h`). Over UDP, the pieces are packed
  into the parts.
- Names, levels and the most health a unit can have go out once a second, with every twentieth world
  (`kFullEvery`), and the client keeps the last ones it had in between. Protobuf leaves out a field
  that's empty or zero, and no name or level is.

With bots that spread over the island, `bench/load.sh 100 12` on an Apple M4 Pro, with the bots on the
same machine as the server:

| Each player gets | 20 ticks a second up to | At 1,200 players | Sent to each player, with 500 |
|---|---|---|---|
| The whole world | 800 players | 10.4 ticks a second | 377 KB/s |
| Within 96 m | 1,000 players | 15.5 ticks a second | 228 KB/s |
| Within 64 m | 1,200 players | 20, ticks of 36 ms (p50) | 135 KB/s |
| Within 64 m, names and levels once a second | 1,200 players | 20, ticks of 26 ms (p50) | 94 KB/s |

On a VPS with 2 vCPUs, with the bots on it too, the game server now keeps its 20 ticks a second up to
about 700 players, where it fell behind past 400.

## What happens near you

The world comes each tick, and the newest replaces the last, so a client knows how things are, but
not what happened in between: who hit whom, for how much, or that a potion was drunk at the moment a
skeleton hit. Those are events, which come once each, and the client floats each over the unit it's
about, as combat text.

- The world records a hit a player lands, a hit a monster lands on a player, health a potion or a
  skill gives back, and a new level, as events (`server/world.h`, `Event`), with the player's id,
  the monster's id for a hit, and the amount. They're the analytics' events too: the analytics keep
  the kinds they count (`ForAnalytics`), and not the blows, which come a second or so apart for each
  player in a fight.
- After the world goes out, the server sends each player the events in their copy of the land whose
  cell is near theirs (`Near` in `server/interest.h`), the same cells they get the units of, in a
  single message a tick, over TCP, so that none of it is lost (`Event` in the protocol). An event
  goes after the world, so that a client getting the world over TCP has the units it's about
  first. Over UDP, an event can come before its world, and the client draws its number once the
  unit has come.
- The client (`client/combat_text.h`) shows a hit's damage over the monster, in white, a monster's
  over the player it hit, in red, health given back in green, and a new level in gold, each rising
  over the nameplate for a second and a half and fading. What happens to the other players is
  smaller than what happens to you. A kind of event from a newer server shows nothing.
- A dead monster has no nameplate, and a bot hunting it thinks again as soon as its world shows it
  dead, so it doesn't swing at the body.
