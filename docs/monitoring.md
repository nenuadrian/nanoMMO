# Monitoring, logs and analytics

## Monitoring and load

Each world serves its metrics for Prometheus at http://localhost:9464/metrics: the players online in each land,
how long each tick takes, how long saving the players takes in each tick, the bytes it sends, the
messages it receives, the lines of chat, said or refused, and the stories asked for, told, refused
or failed, with how long each one took. The bots serve theirs on
port 9465: how old the world on a player's screen would be, by transport.

`compose.yaml` starts the servers, MySQL and Redis together with Prometheus, which finds the worlds
in Consul and scrapes each of them every two seconds, with its name as the `world` label, and
Grafana, with a dashboard of those metrics, world by world. The dashboard and Grafana's data source are files in
`monitoring/`, so they're set up the same way on any machine.

```bash
docker compose up -d --build
```

- Grafana: http://localhost:3000, which opens on the nanoMMO server dashboard.
- Prometheus: http://localhost:9090.
- Pyroscope: http://localhost:4040 (see [Benchmarking and profiling](benchmarking.md)).

`nanommo_bot` connects a swarm of bots that play like players. Each one runs to a random place on the
island, within 170 metres of the middle, or on one trip in five into its land's gate, and wanders around there like a player at the keys, running a
third of the time, for 5 to 15 directions of half a second to 3 seconds each, before it sets off
again. When a bot gets where it was going, it says something, one time in four, from a few
lines. Once it has seen its land's storyteller, one trip in eight goes there instead, where it asks
for a story about three words from the game. While it wanders around somewhere, one direction in
four uses one of the skills it knows, from its world, so that the bots' cooldowns go out with their
worlds, as players' do. A bot drinks a potion once it's down to half its health, one every ten
seconds, and wears what it finds in a slot it has free, so that the servers save and send bags as
they would players'. Quests come first: between trips, a bot looks at what's around it, put
together from the world's parts as the client does, and turns in a quest it's done if its giver is
there, or goes after the nearest monster a quest it's on asks for, or takes a quest it hasn't done
from an NPC there who gives it. It runs there, and swings, or takes or turns in the quest, so that
the servers count kills and save quests as they would players'. A bot that stops
getting closer, at the sea or a lake, gives up on its trip. Each one logs in as bot1, bot2 and so on,
and creates its account the first time. It takes the login server's
host, the game server's host, how many bots, how many seconds to spread their arrival over, the
number of its first bot, so that several swarms can play on one server, and the transport for the
worlds, `udp` or `tcp`, UDP by default:

```bash
./build/bin/nanommo_bot localhost localhost 100 10
./build/bin/nanommo_bot localhost localhost 100 10 101 tcp
```

About sixty times a second, like a client's frames, the bots look at how old their newest world is,
from the time the server puts in each world, and Prometheus finds them through Docker's DNS, under the
name `bots`. The bots and the server have to share a clock for that, as they do on one machine.

Or in Docker, next to the server: 300 bots, arriving over two minutes, unless `BOTS` and `RAMP` say
otherwise.

```bash
docker compose up -d bots
BOTS=1000 RAMP=60 docker compose up -d bots
```

A few hundred bots need more open files than macOS allows a shell by default, so on the Mac, run
`ulimit -n 4096` first. Stop everything with `docker compose --profile load down`.

## Alerts

Prometheus checks the rules in `monitoring/alerts.yml` every five seconds, and its alerts page,
http://localhost:9090/alerts, shows each one as inactive, pending or firing:

- `WorldFallingBehind`, when a world has run fewer than 19 ticks a second, out of its 20, for a
  minute. A world that keeps up runs a little either side of 20.
- `WorldDown`, when a world that was one of Prometheus's targets in the last day hasn't been up for a
  minute. A world that stops leaves Consul, and Prometheus stops scraping it, so `up == 0` would miss
  it. A world whose tick is stuck stops its heartbeat too, so Consul drops it, and `WorldDown` fires.

Prometheus sends the alerts to Alertmanager, at http://localhost:9093, which emails each one when it
fires and when it resolves. The mail goes to Mailpit, a mail server for development, which keeps it,
and shows it at http://localhost:8025 and through its API:

```bash
curl -s localhost:8025/api/v1/messages | jq -r '.messages[].Subject'
curl -s localhost:8025/api/v1/message/latest | jq -r .Text
```

To get the mail somewhere else, `smtp_smarthost` in `monitoring/alertmanager.yml` points at a real mail
server, with `smtp_auth_username` and `smtp_auth_password_file`. The rules have tests, in
`monitoring/alerts_test.yml`, which CI runs too:

```bash
docker compose run --rm alerts
```

## Logs

Each server writes what it does to its output: who joins and leaves, what they say, what they
learn, and what goes wrong. `docker compose logs` shows that a service at a time, and loses it with
the container. [Loki](https://github.com/grafana/loki), Grafana's log store, keeps the logs of all
the game's containers, and Grafana queries them next to the metrics:

- Loki runs in a single process (`monitoring/loki.yaml`), with the logs on a volume of its own,
  `loki`, and keeps them for a week. It indexes each line by its labels, and not by what it says, so
  that keeping logs costs little, and a query picks the lines by their labels first, then searches
  what they say.
- [Alloy](https://github.com/grafana/alloy), Grafana's agent, finds the game's containers through
  Docker's socket, which Compose mounts into it read-only: the ones in the Compose project
  `nanommo`, and looks for new ones each five seconds. It reads what each one writes, as it comes,
  and sends it to Loki
  (`monitoring/config.alloy`). Each container's lines get its service's name as the `service` label,
  such as `server` or `saver`, and each line the stream it was written to as the `stream` label,
  `stdout` or `stderr`, which Docker keeps apart. Our servers write their errors to `stderr` and the
  rest to `stdout`, so `stream="stderr"` picks their errors. Other images use `stderr` for
  everything, such as MySQL's.
- Grafana has Loki as a second data source, so Explore, at http://localhost:3000/explore, searches
  the logs with LogQL, Loki's query language:

```
{service="server"}                       # the world server's lines
{service="server"} |= "joined"           # the ones that say joined
{service=~"server|login|saver", stream="stderr"}   # what went wrong
```

The server dashboard has two panels from Loki at the bottom, under the metrics: errors a minute,
the servers' `stderr` lines counted a minute at a time by service with LogQL's `count_over_time`,
and what the servers say, their lines as they come, newest first. A spike on a chart and the lines
from that minute are on one page. Grafana marks each line with its `level` label, which Alloy gives
our servers' lines from their stream, `error` from `stderr`, and `info` from `stdout`.

Loki checks rules on the logs too, like Prometheus checks its rules on the metrics, every 15
seconds, from `monitoring/loki-rules.yml`, and sends the alerts to the same Alertmanager, which
emails them (see [Alerts](#alerts)):

- `SaverFailing`, when the saver has written errors for a minute, such as while MySQL is down. The
  saver keeps going, and the players it couldn't save stay unsaved in Redis until it can, so the
  game doesn't notice, and nothing in the metrics shows it either.

Loki has no tenants of its own here, so its rules are in its tenant's folder, `fake`, the name Loki
gives the tenant when `auth_enabled` is off.

CI checks both configs, as a typo in either would leave us without logs, and Loki and Alloy refuse
a field they don't know:

```bash
docker compose run --rm loki-config      # loki -verify-config
docker compose run --rm alloy-config     # alloy validate
```

Neither looks at the rules' LogQL. A rule Loki can't parse shows up in its log, as `unable to list
rules`, and the rest of its group doesn't load: `docker compose logs loki | grep rules`.

In production, Loki listens on the server's localhost, like Prometheus, and Alloy has no port.

## Analytics

The metrics say how the servers are doing, and the game's analytics say what the players do: who
killed what, what dropped, which quests they take and which they finish, the levels they reach,
what they drink and where they die. Each of those is an event, a row in
[ClickHouse](https://github.com/ClickHouse/ClickHouse), a database that keeps each column on its own
and reads the columns a query asks about and nothing more, so that counting millions of rows by a
few of their columns is quick.

- Each world records what happens as it steps, as events, with who, which land and where, and what
  it's about by its ids in the content (`server/world.h`, `Event`). Once a tick, the server turns the
  kinds the analytics keep into rows, each a line of JSON
  with the time, the world and the name of what it's about, and adds them to a Redis stream,
  `events`, in a single round trip (`server/events.h`). The stream keeps about the newest million,
  so that it can't fill Redis while nothing reads it, and the tick never waits on ClickHouse.
- The recorder (`nanommo_recorder`) takes the events from the stream, as a consumer of the
  `recorder` group, up to 10,000 at a time, and inserts them into ClickHouse's `events` table in
  its JSONEachRow format, over its HTTP interface, then acknowledges them. Redis remembers what each
  consumer took and didn't acknowledge, so after a failure, or a restart, the recorder takes those
  again first, and an event isn't lost while ClickHouse is down.
- ClickHouse makes the table from `analytics/events.sql` the first time it starts, on an empty
  volume, `clickhouse`: sorted by the kind of event and its time, a partition a month, and kept for
  a year. A change to the table later takes an `ALTER TABLE` of its own.
- Grafana has ClickHouse as a data source, through Grafana's ClickHouse plugin, which it installs
  as it starts, and a second dashboard, nanoMMO game, at http://localhost:3000/d/nanommo-game: the
  players who did something, kills a minute by monster, the quests taken against the ones done, the
  items that came into the game against the ones used, where players die in the Meadows, and the
  levels reached.

```bash
docker compose up -d --build recorder grafana
docker compose exec clickhouse clickhouse-client --user nanommo --password nanommo --database nanommo
```

```sql
SELECT kind, count() FROM events GROUP BY kind;
SELECT what, countIf(kind = 'take') AS taken, countIf(kind = 'turn_in') AS done
FROM events WHERE kind IN ('take', 'turn_in') GROUP BY what;
```

In production, ClickHouse listens on the server's localhost, like Loki.
