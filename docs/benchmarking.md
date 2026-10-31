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
