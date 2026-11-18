# Benchmarking and profiling

`bench/load.sh` measures how the server copes as players arrive. With the stack from `compose.yaml`
running, it adds bots in steps, 250 at a time by default, and after each step it asks Prometheus about
the last 25 seconds: the players online, the ticks the server managed each second, the tick's p50 and
p99, the megabytes it sent each second, and the p99 of saving the players. It needs `curl` and `jq`.

```bash
docker compose up -d --build
mkdir -p bench/results
bench/load.sh 250 10 > bench/results/before.json
```

The server keeps up while it manages 20 ticks a second. Once a tick takes longer than its 50
milliseconds, the next one starts late, and the world runs slower than real time. The numbers depend
on the machine, so compare runs on one machine, with nothing else busy.

To see where the time goes, `compose.profile.yaml` runs the server from the `profile` image instead:
the server built with Tracy's client, next to perf and Brendan Gregg's flame graph scripts. perf
needs two capabilities in the container: `PERFMON` to sample the server and the kernel, and `SYSLOG`
to read the kernel's symbols. Everything is built with frame pointers, so perf can walk the stacks.

```bash
export COMPOSE_FILE=compose.yaml:compose.profile.yaml
docker compose up -d --build server
BOTS=1250 RAMP=60 docker compose up -d bots
docker compose exec server profile.sh before 10
```

`COMPOSE_FILE` makes each compose command in that shell read both files, the override after the
base. Without it, `docker compose up -d bots` would start the server from `compose.yaml` again.

That samples the server for ten seconds and writes `profile/before.svg`, a flame graph to open in a
browser, along with the collapsed stacks it's drawn from.

[Pyroscope](https://github.com/grafana/pyroscope), Grafana's store for profiles, keeps the servers'
profiles all the time, the way Prometheus keeps their metrics, so there's no image to swap and no
moment to catch. Alloy samples the processes of our servers' containers from the kernel, with eBPF,
and sends Pyroscope what was running, as stacks, each fifteen seconds, with C++'s names demangled
(`monitoring/config.alloy`). The servers' code doesn't change. Loading the program into the kernel,
and reading the other containers' processes, takes privileges, so Alloy runs privileged, in the
host's process namespace; on a Mac, the kernel is Docker's Linux VM's. Grafana has Pyroscope as a
data source, and the server dashboard ends on a flame graph of where the worlds spend their CPU over
its time range, under their metrics and logs. Pyroscope's own page is at http://localhost:4040.

When the bots run on the same machine as the server, part of what the flame graph shows is the bots'
cost: sending a datagram over Docker's network, the kernel delivers it to the bot's socket in the
server's own system call, under `udp_sendmsg`. Players on other machines cost the server only the
sending.

[Tracy](https://github.com/wolfpld/tracy) shows each tick as a frame, with the time spent in the
step, the broadcast, and each client's serialize and send. Its client is in the profile image only,
and records nothing until the profiler connects. The profiler has to be the release the server is
built with, 0.14.1: on a Mac, `brew install tracy`, then connect to `localhost`, port 8086.

The broadcast used to serialize the whole world once for each client, so its work grew with the square
of the players. Now it serializes each player and monster once a tick, and the sessions write those
bytes, followed by each client's own id. Most of the tick goes on the kernel sending the bytes, packet
by packet, which is why each player gets what's near them, and no more (see
[What each player gets](networking.md#what-each-player-gets)).

## Chaos experiments

`bench/chaos.sh` breaks one thing on purpose, while bots play, and reports what the game did: what
[Reliability](reliability.md) says about the objectives, and what each experiment found. With the
stack from `compose.yaml` running, the monitoring included:

```bash
docker compose up -d --build login server frost saver prometheus loki tempo alloy grafana alertmanager mailpit
mkdir -p bench/results
bench/chaos.sh redis-pause 5 > bench/results/redis-pause.json
```

It starts `BOTS` bots, 100 by default, arriving over `RAMP` seconds, 30, and waits until the game's
steady: both worlds run 20 ticks a second, the bots' newest worlds are under 200 milliseconds old,
all of them are in, no item has a copy, and the saver's last good flush is under 15 seconds old.
Then it puts the fault in, while `PROBES` more bots, 10, log in, as players keep coming, takes it
out after the seconds given, and waits until the game's steady again, up to three minutes. The
report has, for the window from the fault to fifteen seconds after that, by when Prometheus has read
each service again, even one that was frozen:

- `ticks`: by world, the ticks it owed, 20 a second, ran, ran within 50 milliseconds, missed (owed
  but not on time) and ran late, from Prometheus's samples and their timestamps;
- `players_dropped`, by world; the bots' world's age, at its 99th percentile, at its worst;
  the fewest bots connected, and how many were after;
- `logins` as the login server counted them, by result, with those over five seconds, or null
  when Prometheus couldn't read the login server at either end, and `bot_logins` as the bots did;
- how far behind the saver got, the most items held twice, the spans dropped, the lines on standard
  error by service, in the window and in as long before it, and the alerts that fired or were
  pending;
- the traces Tempo kept with an error, and the slowest, with their ids, which open in Grafana's
  Explore, or at http://localhost:3200/api/traces/<id>;
- `recovered_after_s`, from taking the fault out to steady again, and a timeline, every two
  seconds, of each world's ticks a second, the bots' world's age and the bots connected.

The experiments, and how long each fault lasts by default:

| Experiment | Fault | Seconds |
|---|---|---|
| `redis-pause` | Redis hangs: `docker pause` freezes its container | 5 |
| `redis-down` | Redis stops, and starts again | 7 |
| `mysql-pause` | MySQL hangs | 15 |
| `mysql-slow` | MySQL answers `DELAY` milliseconds late, 50, through netem in its network | 60 |
| `saver-kill` | MySQL hangs, the bots leave, and the saver's killed as it waits with them; the report counts the bots whose rows in MySQL differ from Redis, `stale_in_mysql` | 8 |
| `login-pause` | The login server hangs | 20 |
| `consul-down` | Consul stops, and starts again | 30 |
| `telemetry-pause` | Alloy hangs, so no logs or spans get through | 60 |

`KEEP_BOTS=1` leaves the bots running for the next experiment, which saves their ramp. Each run marks
itself on the reliability dashboard, http://localhost:3000/d/nanommo-reliability, as an annotation.

It refuses a stack started with `compose.prod.yaml`, which `.env` names on a server, and Docker on
another machine, `DOCKER_HOST`: production is never where we break things on purpose. A fault takes
itself out as the script ends, however it ends, and should the script be killed, a backstop takes
it out a little after it should have ended. `bench/chaos.sh restore` puts back whatever's left:
paused containers, a delay, stopped services, a killed run's backstops, and the bots.
