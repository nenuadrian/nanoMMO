# Monitoring, logs, traces and analytics

## Monitoring and load

Each world serves its metrics for Prometheus at http://localhost:9464/metrics. They cover the
players online in each land, how long each tick takes, how long saving the players takes in each
tick, the bytes it sends, the messages it receives, the lines of chat, said or refused, and the
stories asked for, told, refused or failed, with how long each one took, the players it dropped
without saving them, by why (`nanommo_players_dropped_total`), and the war kills it handed the
saver, for their wars' scores (`nanommo_war_kills_total`), which the scores in MySQL should add up
to. The bots serve theirs on port 9465, measuring how old the world on a player's screen would be,
by transport, and how long each bot's login took, by what came of it, timeouts too
(`nanommo_bot_login_duration_seconds`). The saver serves its on port 9466. They cover
`nanommo_duplicate_items`, the serials that more than one item in MySQL has, after its last flush,
which is zero unless an item's been copied (see [Saving players](saving.md)), when its last good
flush ended, and the players waiting for the next. The login server serves its on port 9467,
measuring how long it took to answer each request, by what came of it, ok, refused or failed
(`nanommo_login_duration_seconds`). Prometheus finds those two by their names, `saver` and `login`.
Those are what the objectives in [Reliability](reliability.md) are made from.

`compose.yaml` starts the servers, MySQL and Redis together with Prometheus, which finds the worlds
in Consul and scrapes each of them every two seconds, with its name as the `world` label, and
Grafana, with a dashboard of those metrics, world by world. The dashboard and Grafana's data source are files in
`monitoring/`, so they're set up the same way on any machine.

```bash
docker compose up -d --build
```

- Grafana, at http://localhost:3000, which opens on the nanoMMO server dashboard.
- Prometheus, at http://localhost:9090.
- Pyroscope, at http://localhost:4040 (see [Benchmarking and profiling](benchmarking.md)).

`nanommo_bot` connects a swarm of bots that play like players. Each one runs to a random place on the
island, within 170 metres of the middle, or on one trip in five into its land's gate, and wanders around there like a player at the keys, running a
third of the time, for 5 to 15 directions of half a second to 3 seconds each, before it sets off
again. When a bot gets where it was going, it says something, one time in four, from a few
lines. Once it has seen its land's storyteller, one trip in eight goes there instead, where it asks
for a story about three words from the game. While it wanders around somewhere, one direction in
four uses one of the skills it knows, from its world, so that the bots' cooldowns go out with their
worlds, as players' do. A bot drinks a potion once it's down to half its health, one every ten
seconds, and wears what it finds in a slot it has free, so that the servers save and send bags as
they would players'. Quests come first. Between trips, a bot looks at what's around it, put
together from the world's parts as the client does, and turns in a quest it's done if its giver is
there, or goes after the nearest monster a quest it's on asks for, or takes a quest it hasn't done
from an NPC there who gives it. It runs there, and swings, or takes or turns in the quest, so that
the servers count kills and save quests as they would players'. Before its quests, a bot swears
to a faction, each in turn by its number (`FactionFor`, in `bot/factions.h`), unless its world
shows it sworn already. It walks to the faction's herald, where it sees them, or where the NPCs file
stands them in its land, and swears there, or gives up if nobody's there, so the crowd in the
Meadows wears all three colours within a few seconds of arriving (see
[Factions](gameplay.md#factions)). A bot that stops
getting closer, at the sea or a lake, gives up on its trip. Each one logs in as bot1, bot2 and so on,
and creates its account the first time. It takes the login server's
host, the game server's host, how many bots, how many seconds to spread their arrival over, the
number of its first bot, so that several swarms can play on one server, the transport for the
worlds, `udp` or `tcp`, UDP by default, and `war`, which sends the swarm to war. The bots with odd
numbers are in Red, and the even ones in Blue, whose first bots found them, take them to war and ask
the rest in, by name. Until all its side is in, each bot waits at its side's camp, in lines of five
8 metres west (Red) or east (Blue) of where new players come in, or for a minute and a half after
it joined, and then, in their guild, a bot goes for the nearest enemy it can see, before monsters
and quests (`bot/war.h`), so the two sides charge each other in lines.

```bash
./build/bin/nanommo_bot localhost localhost 100 10
./build/bin/nanommo_bot localhost localhost 100 10 101 tcp
./build/bin/nanommo_bot localhost localhost 100 30 1 udp war
```

About sixty times a second, like a client's frames, the bots look at how old their newest world is,
from the time the server puts in each world, and Prometheus finds them through Docker's DNS, under the
name `bots`. The bots and the server have to share a clock for that, as they do on one machine.

A bot that loses its world, say as a world dropped it, logs in again, after a wait that doubles
with each try that fails, from a second to thirty, and a random part of that, so that bots dropped
together don't all come back together, and five a second at most.

Or in Docker, next to the server, with 300 bots, arriving over two minutes, unless `BOTS` and `RAMP`
say otherwise, from bot1, or `BOTS_FIRST`.

```bash
docker compose up -d bots
BOTS=1000 RAMP=60 docker compose up -d bots
```

A few hundred bots need more open files than macOS allows a shell by default, so on the Mac, run
`ulimit -n 4096` first. Stop everything with `docker compose --profile load down`.

## Alerts

Prometheus checks the rules in `monitoring/alerts.yml` every five seconds, the objectives' every 30, and its alerts page,
http://localhost:9090/alerts, shows each of the alerts below as inactive, pending or firing.

- `WorldFallingBehind`, when a world has run fewer than 19 ticks a second, out of its 20, for a
  minute. A world that keeps up runs a little either side of 20.
- `WorldDown`, when a world that was one of Prometheus's targets in the last day hasn't been up for a
  minute. A world that stops leaves Consul, and Prometheus stops scraping it, so `up == 0` would miss
  it. A world whose tick is stuck stops its heartbeat too, so Consul drops it, and `WorldDown` fires.
- `TicksBurningBudget`, `TicksDrainingBudget`, `LoginsBurningBudget` and `SavesBurningBudget`, when
  an objective's error budget is going fast enough to be gone long before the month is, a page, or
  steadily, a ticket, as [Reliability](reliability.md#alerts-on-how-fast-a-budget-burns) explains.
  They and the recording rules they read are in the `slos` group.
- `ItemsDuplicated`, when `nanommo_duplicate_items` has been above zero for a minute. Above zero
  means two items with one serial, an item and its copy, which a dupe makes, as in [decision
  31](decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md). A minute, as a trade that
  the saver took half of, while it was reading a batch, is whole again at its next flush, five
  seconds later. The trades table has who gave whom each item, by its serial.

Prometheus sends the alerts to Alertmanager, at http://localhost:9093, which emails each one when it
fires and when it resolves. The mail goes to Mailpit, a mail server for development, which keeps it,
and shows it at http://localhost:8025 and through its API.

```bash
curl -s localhost:8025/api/v1/messages | jq -r '.messages[].Subject'
curl -s localhost:8025/api/v1/message/latest | jq -r .Text
```

To get the mail somewhere else, `smtp_smarthost` in `monitoring/alertmanager.yml` points at a real mail
server, with `smtp_auth_username` and `smtp_auth_password_file`. The rules have tests, in
`monitoring/alerts_test.yml`, which CI runs too.

```bash
docker compose run --rm alerts
```

## Logs

Each server writes what it does to its output. That's who joins and leaves, what they say, what they
learn, and what goes wrong. `docker compose logs` shows that a service at a time, and loses it with
the container. [Loki](https://github.com/grafana/loki), Grafana's log store, keeps the logs of all
the game's containers, and Grafana queries them next to the metrics.

- Loki runs in a single process (`monitoring/loki.yaml`), with the logs on a volume of its own,
  `loki`, and keeps them for a week. It indexes each line by its labels, and not by what it says, so
  that keeping logs costs little, and a query picks the lines by their labels first, then searches
  what they say.
- [Alloy](https://github.com/grafana/alloy), Grafana's agent, finds the game's containers, the
  ones in the Compose project `nanommo`, through Docker's socket, which Compose mounts into it
  read-only, and looks for new ones each five seconds. It reads what each one writes, as it comes,
  and sends it to Loki
  (`monitoring/config.alloy`). Each container's lines get its service's name as the `service` label,
  such as `server` or `saver`, and each line the stream it was written to as the `stream` label,
  `stdout` or `stderr`, which Docker keeps apart. Our servers write their errors to `stderr` and the
  rest to `stdout`, so `stream="stderr"` picks their errors. Other images use `stderr` for
  everything, such as MySQL's.
- Grafana has Loki as a second data source, so Explore, at http://localhost:3000/explore, searches
  the logs with LogQL, Loki's query language, in queries like these.

```
{service="server"}                       # the world server's lines
{service="server"} |= "joined"           # the ones that say joined
{service=~"server|login|saver", stream="stderr"}   # what went wrong
```

The server dashboard has two panels from Loki at the bottom, under the metrics. One is errors a
minute, the servers' `stderr` lines counted a minute at a time by service with LogQL's
`count_over_time`, and the other what the servers say, their lines as they come, newest first. A
spike on a chart and the lines from that minute are on one page. Grafana marks each line with its
`level` label, which Alloy gives our servers' lines from their stream, `error` from `stderr`, and
`info` from `stdout`.

Loki checks rules on the logs too, like Prometheus checks its rules on the metrics, every 15
seconds, from `monitoring/loki-rules.yml`, and sends the alerts to the same Alertmanager, which
emails them (see [Alerts](#alerts)).

- `SaverFailing`, when the saver has written errors for a minute, such as while MySQL is down. The
  saver keeps going, and the players it couldn't save stay unsaved in Redis until it can, so the
  game doesn't notice, and nothing in the metrics shows it either.

Loki has no tenants of its own here, so its rules are in its tenant's folder, `fake`, the name Loki
gives the tenant when `auth_enabled` is off.

CI checks both configs, as a typo in either would leave us without logs, and Loki and Alloy refuse
a field they don't know.

```bash
docker compose run --rm loki-config      # loki -verify-config
docker compose run --rm alloy-config     # alloy validate
```

Neither looks at the rules' LogQL. A rule Loki can't parse shows up in its log, as `unable to list
rules`, and the rest of its group doesn't load, which `docker compose logs loki | grep rules` shows.

In production, Loki listens on the server's localhost, like Prometheus, and Alloy has no port.

## Tracing

The metrics say the logins' 99th percentile went up, and a server's log says what that one process
did. A trace follows one request through each process it goes through, the login server, Redis,
MySQL, a world and the saver, as spans, each a piece of the work, with when it started, how long it
took, and which span it was part of. So a slow login shows where its time went, and in which
process ([decision 38](decisions/0038-trace-requests-with-a-tracer-of-our-own.md)).

- The login server, the worlds and the saver send their spans, in OpenTelemetry's protocol, OTLP,
  as JSON over HTTP, to the collector that `NANOMMO_OTLP_ENDPOINT` names. Compose sets it to
  Alloy's, http://alloy:4318, and a server run on this machine can use http://localhost:4318, which
  Compose publishes. Without it, a server traces nothing, so the tests and a server run on its own
  need no collector.
- [Alloy](https://github.com/grafana/alloy) keeps the traces worth keeping (see
  [Sampling](#sampling)), and sends them on to [Tempo](https://github.com/grafana/tempo), Grafana's
  trace store, in a single process (`monitoring/tempo.yaml`), with the traces on a volume of its
  own, `tempo`, and its API at http://localhost:3200.
- Tempo makes metrics from the spans as they come, who calls whom and how long each kind of span
  takes, and writes them into Prometheus (`traces_service_graph_*`, `traces_spanmetrics_*`), each
  sample with the trace it came from, an exemplar.
- Grafana has Tempo as a data source. The nanoMMO traces dashboard, at
  http://localhost:3000/d/nanommo-traces, has who calls whom as a graph, the requests a second and
  how long the slowest take, by kind, and the traces that took over 100 milliseconds, each of which
  opens in Explore. Grafana lets anonymous visitors edit, as Explore is an editor's.

The tracer is our own, `server/tracing.h`, which speaks OpenTelemetry's formats, using W3C Trace
Context's `traceparent`, `00-<trace id>-<span id>-01`, for a span's place, and OTLP's JSON for the
spans. A span that's open is its thread's current span, and a span started on the thread goes under
it. A span that ends goes on a queue, and a thread of the tracer's sends what's there once a second,
or as soon as there are 512, so the thread that ended it, which may be the tick's, never waits for
the collector. With 4096 waiting, a span that ends is dropped, and so is a batch the collector
doesn't take, each counted in `nanommo_spans_dropped_total`, by the worlds and the saver. The login
server serves no metrics, so its drops aren't counted. A server that stops sends what's left, for
two seconds at most.

### What's traced

Each trace starts with a request, and the work it does in each process is a span, named by what it
is. The login server's spans are `login`'s, a world's are its instance's, such as `Ember-7777`, as
two instances of a world can run at once, and the saver's are `saver`'s.

| Trace | Its spans |
|---|---|
| A request to the login server | `log in`, `create account`, `enter world`, `create character`, `delete character`, `list characters` or `list worlds`, with `argon2id hash` or `argon2id verify` under a login, which is slow on purpose |
| Entering a world | The login server's `enter world`, its `issue token`, and the world's `hello` under that, with `lease player` and `load player` |
| A trade | The world's `trade`, `save players` and `publish trade`, then the saver's `save trades`, with its transaction in MySQL |
| A handover | The leaving instance's `hand over`, `pass leases` and an `issue token` for each player, and each player's `hello` on the next instance |

Each call to Redis, each statement to MySQL and each request to Consul is a span under the span
that's current as it's made, such as `redis GETDEL`, `mysql SELECT players` or `consul GET
/v1/health/service/nanommo-world`, with `db.system` and `db.operation`, as OpenTelemetry names them.
A statement's span has its SQL, with a `?` for each value, and never a key or a value. A player's
mistake, such as a wrong password, is an attribute, `nanommo.refused`, and the servers' failures,
such as MySQL being down, mark their spans as errors. The lines a server prints as it answers a
traced request end with its trace's id, `trace_id=`.

These aren't traced.

- The tick. It's not a request, and nobody waits on one alone, while a span for each, 20 a second
  in each world, would be 1.7 million a day for what Tracy and `nanommo_tick_duration_seconds`
  already show (see [Benchmarking and profiling](benchmarking.md)). The tick's own calls to Redis
  and MySQL have no span to go under, so they aren't traced either, and neither are the saver's
  flushes that carry no trade.
- Going through a gate. A world's lands are copies in one process, and a player goes through as
  the world steps, in the tick, without a request, or anything over the network.
- The client. Its spans would come from players' computers, whose clocks differ from the servers'
  by up to minutes, through a collector open to the internet, which anyone could send spans to.
  The servers' spans start where their work does, as a request arrives.
- The browser's way in, through the gateway, which carries the client's bytes as they are.

### How a trace goes from one process to another

A trace goes from one process to another in what already carries the request there, as a
traceparent.

- **The token.** The token to enter a world goes from the login server, through Redis, to the
  world, which reads it back as the player says hello. Issuing it is a producer span, whose
  traceparent goes into the token's record, after the player's id, so `token:<token>` holds
  `7 00-…-01`. Redeeming it gives the world that span, and the hello goes under it, so entering a
  world is one trace, across both. A world handing its players over issues their tokens the same
  way, so a deploy is one trace across the two instances. The hello's span starts as its message
  arrives, but it's only once Redis gives the token back that the world knows whose trace it's in.
- **The trades stream.** A trade's record goes on the `trades` stream with a `traceparent` field,
  from its `publish trade` span. The saver carries a batch's trades on in a consumer span, from when
  it read them, under the oldest trade's span, and linked to the others', as a span has one parent
  and a batch is for each of its trades. The gap between the world's half of a trade and the
  saver's is the time its record waited in the stream, up to the saver's five seconds.

A record without a traceparent, from a server that doesn't trace, is read as before, and one that
can't be read loses the trace, not the player. A session from the login server carries no trace,
as it lasts half an hour, through every request a player makes with it, which would make a trace
with no end, so logging in is a trace of its own, and entering a world another.

### Sampling

Alloy keeps some of the traces, not all (`monitoring/config.alloy`). Sampling at the head, as a
trace starts, decides before anything's happened, so it keeps a slow trace, or one that failed, no
more often than the rest, and those are the ones worth reading. Sampling at the tail waits until a
trace is whole, and decides from all its spans, at the cost of holding each one in memory until
then. Alloy waits ten seconds from a trace's first span, as a trade's saver half comes up to five
seconds after its world half, and keeps every trace with an error, every trace that took over 100
milliseconds, and a tenth of the rest, picked by their ids, to see what a normal one looks like.

A span that comes after its trace's decision, such as a hello with a token issued 30 seconds
before, follows it. Tempo makes its metrics from what's kept, so they count a tenth of the quick
traces, and a log line's `trace_id` opens its trace when Alloy kept it.

### Reading traces

In Grafana's Explore, http://localhost:3000/explore, Tempo searches with TraceQL, Tempo's query
language, and a trace opens as a waterfall of its spans, with a button to its servers' lines in
Loki, a minute either side. In Loki, a line's `trace_id` opens its trace, and on a Prometheus chart,
a dot, an exemplar, opens the trace a sample came from.

```
{ name = "hello" && duration > 10ms }               # the slow hellos
{ resource.service.name = "saver" }                 # the trades the saver took
{ traceDuration > 100ms }                           # what the dashboard lists
{ status = error }                                  # what failed
```

Tempo's API answers the same, as JSON.

```bash
curl -sG localhost:3200/api/search --data-urlencode 'q={ name = "hand over" }'
curl -s localhost:3200/api/traces/<trace id>
```

### What the traces showed

In episode 87, with a hundred bots, a player's hello took about a millisecond on the world's
thread, the one that ticks, and most of it was MySQL, with three statements to load the player and
one for their guild, each 0.15 to 0.25 milliseconds against Redis's 0.06. With MySQL's replies
delayed by two milliseconds, each statement took 4.7, two round trips, as `Database::Run` prepared
each statement, a round trip, ran it, another, and closed it. A hello took 19 milliseconds, and a
handover of a hundred players, whose hellos come all at once, kept the next instance busy with them
for 1.95 seconds, in which it didn't tick. Since 0.87.0, a connection keeps the reads it's prepared,
and runs one again in a round trip. With the same delay, each statement takes 2.4 milliseconds, a
hello 9.9, and the handover's hundred hellos 0.97 seconds.

To see that for yourself, netem, from the Dockerfile's `netem` stage, delays what MySQL sends.

```bash
docker build --target netem -t nanommo-netem .
docker run --rm --network container:nanommo-mysql-1 --cap-add NET_ADMIN --entrypoint tc \
  nanommo-netem qdisc replace dev eth0 root netem delay 2ms
docker run --rm --network container:nanommo-mysql-1 --cap-add NET_ADMIN --entrypoint tc \
  nanommo-netem qdisc del dev eth0 root                 # and back
```

In production, Tempo listens on the server's localhost, like Loki, and the servers reach Alloy on
Compose's network.

## Analytics

The metrics say how the servers are doing, and the game's analytics say what the players do. They
say who killed what, what dropped, which quests they take and which they finish, the levels they
reach, what they drink and where they die. Each of those is an event, a row in
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
- ClickHouse makes the table from `monitoring/clickhouse/events.sql` the first time it starts, on an
  empty volume, `clickhouse`. The table is sorted by the kind of event and its time, with a
  partition a month, and kept for a year. A change to the table later takes an `ALTER TABLE` of its
  own.
- Grafana has ClickHouse as a data source, through Grafana's ClickHouse plugin, which it installs
  as it starts, and a second dashboard, nanoMMO game, at http://localhost:3000/d/nanommo-game, with
  the players who did something, kills a minute by monster, the quests taken against the ones done,
  the items that came into the game against the ones used, where players die in the Meadows, and the
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
