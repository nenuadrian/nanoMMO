# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler, and two libraries from your system's package manager:
MySQL's client library, and libsodium for hashing passwords. The client draws with raylib, on top of
the system's OpenGL.

- macOS: `brew install mysql-client libsodium pkgconf`.
- Ubuntu: `sudo apt install pkg-config libmysqlclient-dev libsodium-dev xorg-dev libgl1-mesa-dev`,
  where the last two are the X11 and OpenGL headers.

CMake fetches and builds the other libraries: GoogleTest, raylib, Dear ImGui, rlImGui, abseil,
protobuf, Asio, prometheus-cpp and hiredis. The first build compiles protobuf, so it takes a few
minutes.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The servers keep accounts and players in MySQL, and hand logins over through Redis. Docker Compose
runs both, with the tables from `db/schema.sql`:

```bash
docker compose up -d mysql redis
./build/bin/nanommo_login
```

Then, in another terminal, the game server, and in one or two more, the client:

```bash
./build/bin/nanommo_server
./build/bin/nanommo_client
```

The client opens on a login screen: type a name and a password, then Create account the first time,
and Log in after that. It connects to `localhost`, or to the host you give it
(`./build/bin/nanommo_client 192.168.1.20`). It gets the world over UDP, unless you put `tcp` after the
host (`./build/bin/nanommo_client localhost tcp`); see [Packet loss: TCP and UDP](#packet-loss-tcp-and-udp).
Each player is an animated character on a board of tiles,
with its name over it, or a cube until you've made the characters (see below). W, A, S and D, or the
arrow keys, move yours, which has a white marker over it, and Shift makes it run. View,
Settings changes the background, and Game, Log out takes you back to the login screen. Next time you
log in, you're where you left off.

`protocol/nanommo.proto` is the contract between the client and the servers. The build runs protoc on
it, and all sides use the C++ it generates.

## Characters

The players are Mixamo's X Bot and Y Bot, with an idle, a walk and a run. Mixamo's terms don't let us
share its files, so they're not in the repo: [assets/mixamo/README.md](assets/mixamo/README.md) lists
what to download from mixamo.com. Then make the characters:

```bash
docker build --target characters --output assets/characters .
```

The `characters` stage converts the FBX files to glTF with FBX2glTF, and
[assets/tools/merge.mjs](assets/tools/merge.mjs) puts each character and the clips into one `.glb`
that raylib can load: one skeleton, the walk and the run in place, and a tenth of the triangles. The
build copies `assets/characters` next to the client, which loads them from there. Without them, the
players are cubes, and the client's character tests skip themselves.

The server decides which way each player faces, and whether it stands, walks or runs. It moves them
at 1.8 and 3.1 metres a second, the speeds of the walk and the run, so that the feet don't slide.
The client blends from one clip into the next over a quarter of a second, and smooths each player's
position between the server's ticks.

## Accounts and logins

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it puts a token in Redis, which works once, within 30 seconds, and sends
  it to the client.
- The client takes the token to the game server, `nanommo_server`, on port 7777. The game server takes
  the token out of Redis, and loads your player from the `players` table. It saves where you are
  when you leave, and saves everyone when it stops.

The password goes over the network as it is typed. That's fine on your own machine. Before anyone
plays over the internet, the login server needs TLS.

The servers find MySQL and Redis on `127.0.0.1`, or wherever `NANOMMO_MYSQL_HOST` and `NANOMMO_REDIS_HOST`
point. The tests that need them skip themselves unless those are set:

```bash
NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 ./build/bin/nanommo_server_tests
# Or all in Docker, next to MySQL and Redis.
docker compose run --rm tests
```

MySQL keeps its data in the `mysql` volume, so it survives `docker compose down`. MySQL runs
`db/schema.sql` the first time it starts, on an empty volume. `docker compose down -v` deletes the
volume, and with it all the accounts.

Or with only Docker installed:

```bash
# Build and run the tests on Ubuntu.
docker build --target test .
# Check the code style.
docker build --target lint .
# The server image, if the tests pass.
docker build --target server -t nanommo-server .
# The login server's image.
docker build --target login -t nanommo-login .
# API docs in site/.
docker build --target docs --output site .
```

In Docker, the client's tests open their window on a virtual display from Xvfb, and render with
llvmpipe, Mesa's OpenGL driver that runs on the CPU. So they don't need a screen or a GPU.

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.

## Monitoring and load

The server serves its metrics for Prometheus at http://localhost:9464/metrics: the players online,
how long each tick takes, the bytes it sends and the messages it receives. The bots serve theirs on
port 9465: how old the world on a player's screen would be, by transport.

`compose.yaml` starts the servers, MySQL and Redis together with Prometheus, which scrapes the game
server every two seconds, and Grafana, with a dashboard of those metrics. The dashboard and Grafana's data source are files in
`monitoring/`, so they're set up the same way on any machine.

```bash
docker compose up -d --build
```

- Grafana: http://localhost:3000, which opens on the nanoMMO server dashboard.
- Prometheus: http://localhost:9090.

`nanommo_bot` connects a swarm of bots that wander around like players at their keys, running a third of
the time. Each one logs in as bot1, bot2 and so on, and creates its account the first time. It takes the login server's
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

## Benchmarking and profiling

`bench/load.sh` measures how the server copes as players arrive. With the stack from `compose.yaml`
running, it adds bots in steps, 250 at a time by default, and after each step it asks Prometheus about
the last 25 seconds: the players online, the ticks the server managed each second, the tick's p50 and
p99, and the megabytes it sent each second. It needs `curl` and `jq`.

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

[Tracy](https://github.com/wolfpld/tracy) shows each tick as a frame, with the time spent in the
step, the broadcast, and each client's serialize and send. Its client is in the profile image only,
and records nothing until the profiler connects. The profiler has to be the release the server is
built with, 0.14.1: on a Mac, `brew install tracy`, then connect to `localhost`, port 8086.

The broadcast used to serialize the whole world once for each client, so its work grew with the square
of the players. Now it serializes the players once a tick, and all the sessions write those bytes,
followed by each client's own id. What's left grows with the square of the players too, as each
client still gets all of them: that's the kernel sending the bytes.

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
- The players go out in parts of up to 1,200 bytes, so that each fits in a packet and IP doesn't split
  it into fragments, which would all be lost with any one of them. A lost part leaves its players where
  they were for a tick. `protocol/parts.h` splits the world and puts it back together.
