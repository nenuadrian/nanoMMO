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
about 700 players, where before it fell behind past 400.

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

## Your own moves, before the server answers

The server decides where everyone is, so with nothing more, a key you press moves you a round trip
later: at 150 milliseconds, you start and stop walking a beat after your fingers. The client walks
you at once instead, the way the server will, and the server's answer corrects it.

| Where | What |
|---|---|
| The client | Sends its keys once each of the server's ticks, 20 a second, each numbered, from 1 |
| The server | Applies a player's numbered keys in order, one a tick, before the world steps, keeping a few that arrive together and dropping the oldest past five. A slow frame on the client sends two or three together, so with more than three waiting, it's fallen behind the client, and skips one a tick until it's caught up. Inputs come in as fast as they go out, so a queue a burst filled would stay full: if two or more waited at the start of every tick for a second, it drops one |
| Each world | Says the last of your inputs the server applied (`acked`), and whether it moved you itself that tick (`steered`), with where you are, in every datagram |
| The client | Shows the round trip under the minimap: from sending an input to the world that says it was applied |

- The client walks you with each tick's keys as it sends them, with `WalkAlong` from
  `server/walking.h`, which the server walks players with. It's a library both link, built without
  fusing a multiply and an add, like the terrain, so a step comes out the same on every machine.
- Each world puts you where the server has you, after the last input it applied, and walks you
  again through the ones it hasn't (`client/prediction.h`). When the two agree, nothing moves; when
  they don't, the server wins, and what the correction moved fades over a tenth of a second, unless
  it's a jump of two metres or more, as through a gate.
- While the server moves you itself, along a walk you asked for, up to a monster, with a skill, or
  you're dead, your keys don't walk you on the client either, and you're drawn moving as the server
  says.
- You're drawn where the prediction has you, walking as your keys say. Everyone else is drawn a
  moment in the past (below).

To feel it, put a delay on the server's packets with the `netem` service above, and walk.

## Everyone else, a moment in the past

Worlds come 20 times a second, but not evenly: one can be late, two can come together, and over
UDP one can be lost. Drawing each player and monster where the newest world has them shows that as
a stop and a lurch. So the client draws everyone else as they were a moment ago, between two worlds
it already has (`client/interpolation.h`).

- It keeps the last second of worlds, each with the time the server sent it, on the server's
  clock, and the time it came, on the client's. A world's other parts fill it in as they come.
- The two clocks needn't agree. The world that came quickest says best how far apart they are, as
  the others were held up on the way by that much more.
- It draws everyone else as they were two ticks ago, 100 milliseconds, on the server's clock,
  between the two worlds on either side of then. Two ticks is enough for a world up to a tick late.
- With no world after then, as when the next one is lost, the last two carry them on the way they
  were going, two ticks at most, and then they wait for it.
- You're the prediction's (above), so you're drawn now, and everyone else 100 milliseconds ago:
  that's the price of seeing them move smoothly.

## Only what's changed

Each world used to carry everyone near you, whole, 20 times a second, so a player standing in a
crowd cost the same as one walking through it. Now each world leaves out what hasn't changed since
the newest world your client has.

- Each input says the newest world the client has whole, every part of it, by when it was sent
  (`seen`), and the bots' say so too, each tick, as a client's do.
- The server remembers a second of what it sent each client: the copy of the land it was in, and a
  hash of each player's and monster's piece (`Server::Delta`).
- A player or monster whose piece is the same as in the world the client has goes by its id alone,
  in `still_players` or `still_monsters`. The client keeps those as it has them, and as still near
  it. Anyone in neither, in a world that came whole, is gone, as before.
- A world the server no longer remembers, or from another copy of a land, whose ids are its own, is
  no help, and everything goes whole, as before. So does a world to a client that says nothing.
- Each client's world is its own now, so the clients in a cell no longer share their datagrams.

With forty bots on one machine (`nanommo_bot localhost localhost 40 5 1 udp`), the server sent
16.8 KB a second to each before, and 7.6 KB after, from `nanommo_bytes_sent_total`. Those who
move still go whole each tick: a player walking changes where they are, and so their piece.

## A duel's messages

A duel ([Gameplay](gameplay.md#duels)) is the first time what one client sends hurts another
player, so the server keeps it to the two who said yes, and checks the rest as it does a trade.

| Message | Field | What |
|---|---|---|
| `Input` | `duel`, 19 | An ask, by name, a yes, a no or a yield, once, over TCP |
| `Input` | `target_player`, 20 | The player picked, with each input, beside the monster's `target`. The world keeps it only for the opponent in a duel that's started, or an enemy at war (below), so it can't hurt anyone else |
| `World` | `duel`, 27 | Who with, by name and id, how far it's got, the countdown's milliseconds and the flag. On its own, over TCP, to both sides, as it's asked for, at each second of the countdown, and as it starts and ends, with nobody in it once it's over |
| `Player` | `target_player`, 16 | While they use a skill in a duel, the player it's for, so that its fx land on them on everyone's screen, as `target` does for a monster |
| `Input` | `drawn`, 21 | When, on the server's clock, in microseconds, the client was drawing everyone else as it sent it, with each numbered input: a blow on the opponent is checked against where they were then (below) |

## A war's messages

A guild war ([Gameplay](gameplay.md#guild-wars)) lets the players of two guilds hurt each other
without a duel, so `Input`'s `target_player`, 20, is kept for a player of the guild at war with
yours too, and a blow on them is checked as a duel's is (below).

| Message | Field | What |
|---|---|---|
| `GuildAction` | `war`, 11 | A leader's ask, by the other guild's name, or a yes, as asking back is, once, over TCP |
| `Guild` | `war`, 4 | The guild's war, with its news, a `GuildWar`: the enemy guild, by name, `kills` and `enemy_kills`, from your side, as MySQL has them, a few seconds behind, and `left`, the seconds it had left, which the client counts down from. Nothing, for no war |

The news comes as the war starts, as its score moves, and as it ends, as the world reads its wars
every five seconds. Each kill is a line from nobody in both guilds' chat, on `CHANNEL_GUILD`, which
the client shows in green, after `[Guild]`.

## Blows where the attacker saw them

A client draws everyone else 100 milliseconds in the past (above), and its keys reach the server a
round trip after the world they answer left it, so by the time a blow lands, the player it was
aimed at has moved on from where the attacker saw them. Running, at 3.1 metres a second, with a
round trip of 50 to 200 milliseconds, that's 0.5 to 0.9 metres, against a sword's reach of 1.6: a
blow that looked in reach misses, and one that looked as if it would miss can land. Making up for
it is called lag compensation, and the server does it for the blows of a duel or a war.

- Each World keeps where each of its players is after each step, for the last second, by a clock of
  its own that goes on a step at a time (`server/history.h`). It gives where they were at any time
  in that second, between the ticks on either side.
- Each numbered input says when, on the server's clock, its client was drawing everyone else as it
  sent it (`drawn`): 100 milliseconds before the newest world, as the clocks go
  (`Interpolation::Drawn`). As the input is applied, the server takes how far behind its own clock
  that is.
- A player's blows on their duel opponent, or an enemy at war, are checked against where that
  player was that long before: the reach and the arc, and the aim and the run-up, which go by where
  the one picked is. Nobody's moved: the history is only looked at.
- A client can say anything, so the server believes 200 milliseconds at most
  (`Inputs::kMostRewind`): the 100 a client draws everyone in the past, and a round trip of up to
  100. A time further back is 200 milliseconds, and a time ahead of the server's clock is now, as is
  an input that doesn't say, as a bot's.
- A monster is hit where it is, as a blow pushes and throws it from there. It stands still to swing,
  and comes to whoever hurt it, so where a client drew it a moment ago is near enough.

The cost falls on the player who's hit: running off, out of reach on their own screen, they can
still be hit where they were up to 200 milliseconds before, 0.62 metres behind them at a run. A
client can always say it's the most behind, so a modified one gets that whatever its network, about
100 milliseconds more than a client beside the server (`stale`, below). Over a round trip of 100
milliseconds, a player has to lead a runner a little, as everyone did before. Why it's 200
milliseconds, and duels only, is
[decision 34](decisions/0034-duel-blows-are-checked-where-the-attacker-saw-the-target.md).

To see it, put a delay on the server's packets with the `netem` service, start a duel, and have
the other player run out of reach as a blow comes.

## The sky

Each player's own part of the world carries the sky over their land, `World`'s `sky` (28): the
time of day, the weather, how strong it is, and the wind ([Graphics](graphics.md#day-and-night),
[decision 0037](decisions/0037-the-server-owns-the-sky.md)). The server works it out from the wall
clock (`server/weather.h`), so it's nothing to keep, and it's about 25 bytes, so it goes in every
world, and a client that loses one has it again a twentieth of a second later. `Parts` keeps the
newest world's.

## What a modified client can try

The client runs on the player's machine, so anything it sends can be made up. `nanommo_cheat` is
the server's red-team client: a player without a window, who breaks the client's rules one way at a
time and prints what it gets for it (`cheat/main.cc`). It runs beside the servers, with an account
of its own: `nanommo_cheat <name> <password> <cheat>`, and for a trade, a duel or a war, the
partner, who runs `take` for a trade, for a duel says yes in their client, or runs `duel`, and for a
war runs `feed`: `nanommo_cheat <name> <password> <cheat> <partner>`.

| Cheat | What it sends | What it gets |
|---|---|---|
| `speed` | Three running inputs a tick, where the client sends one | 3.1 m/s, as running does: the server applies one input a tick (`ServerTest.MoreInputsThanTicksDontRunFaster`) |
| `teleport` | A direction of 1000, then one that isn't a number | 3.1 m/s: a direction is scaled back to a length of one, and one that isn't a number is ignored, so the last keys go on |
| `reach` | A swing at a monster 20 metres away | No hit from there: the server runs the player up to the monster first |
| `rapid` | Its first skill ten times a tick | No more hits than using it once it's ready: the cooldowns are the server's |
| `cancel` | Its weapon into the bag as each blow lands, and back two ticks later | Nothing, from 0.72.0: a skill keeps the timings of the weapon it started with, so a swing whose blow has landed plays out (`World.PuttingTheWeaponAwayAfterItsBlowDoesntSwingSooner`). Before, the next swing started as soon as the weapon was back, about twice as often |
| `swap` | Its sword offered from the bag, and once the partner's ready, a move of the sword to another slot, and its own ready | Nothing, from 0.75.0: a place that's offered is held until the trade's over, so the move is refused, and the partner gets the sword (`Trades.WhatsOfferedStaysWhereItIsUntilTheTradesOver`). The trade gives nothing unless each place still has what was offered (`Trades.AnItemMovedAfterItsOfferedIsntGivenTwice`). Before, the partner got the copy made at the offer, and the sword stayed too: three swords from two |
| `run` | Its sword offered, and once the partner's ready, its own ready, and it leaves at once | Nothing, from 0.75.0: a trade saves both bags, together, as it's done, and a player who leaves is saved as they go, so it comes back without the sword (`ServerTest.ATradeAndALeaveLeaveOneSword`). Before, it left before the tick that would have saved its bag, and came back to the bag Redis had from before the trade: three swords from two |
| `twice` | A hello to the world's second instance too, with a token of its own, each second for twenty seconds until it's let in, and its sword traded away on the first, which it leaves. On the second, which loaded its bag before the trade, a move of the sword into the bag | Nothing, from 0.79.0: a character plays on one instance at a time, the one that has their lease, so the second turns it away, and the trade goes as it should (`ServerTest.ACharacterPlaysOnOneInstanceAtATime`). Should the first stall for longer than a lease, the second can take it, and the first can't save it any more, and drops it (`ServerTest.AnInstanceDropsAPlayerWhoseLeaseItsLost`). Before, each instance saved it as it had it, and the second saved its bag from before the trade over the trade: three swords from two |
| `stale` | In a duel, with each input, that its client drew everyone a second before its newest world, where a client says 100 milliseconds, to hit its opponent where they were a second ago | No more than 200 milliseconds back, from 0.82.0, which the server believes at most (`Inputs.AClaimFurtherBackThanTheMostIsTheMost`), so its blows land within reach of where its opponent was then, and not of where they were a second ago (`ServerTest.ADuelBlowLandsWhereTheClientDrewThemUpToTheMost`). It prints what it says, with how far off its opponent was then and is now, and each blow that lands. That's 100 milliseconds more than a client beside the server says, 0.31 metres on a runner |
| `farm` | In a war with the partner, a friend in the other guild, who runs `feed`: stands still until it's killed, comes back as soon as it can and walks back. `farm` picks them and swings each tick | 2 kills of 17 scored in six and a half minutes, from 0.92.0: a victim scores for a war once every five minutes at most (`Wars.AVictimScoresOnceEveryFiveMinutes`). The kills still happen. Before, each one scored: 15 to 0 in six minutes, 2.6 a minute, for as long as the war lasts |

`walk`, `fight`, `swing`, `take`, `duel` and `feed` keep the rules, to compare with. Why the server
checks everything is [decision 25](decisions/0025-the-server-checks-what-a-client-asks-for.md), how
a trade is checked and saved, [decision 28](decisions/0028-a-trade-is-checked-against-the-bags-and-saved-as-one.md),
why a character plays on one instance at a time,
[decision 31](decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md), and why a victim
scores once every five minutes, [decision 42](decisions/0042-a-war-kill-counts-once-and-nobody-leaves-a-war.md).
