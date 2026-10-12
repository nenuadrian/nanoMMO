# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler, and three libraries from your system's package manager:
MySQL's client library, libsodium for hashing passwords, and OpenSSL for TLS. The client draws with
raylib, on top of the system's OpenGL.

- macOS: `brew install mysql-client libsodium openssl@3 pkgconf`.
- Ubuntu: `sudo apt install pkg-config libmysqlclient-dev libsodium-dev libssl-dev xorg-dev
  libgl1-mesa-dev`, where the last two are the X11 and OpenGL headers.

CMake fetches and builds the other libraries: GoogleTest, raylib, Dear ImGui, rlImGui, abseil,
protobuf, Asio, prometheus-cpp and hiredis. The first build compiles protobuf, so it takes a few
minutes.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The servers keep accounts in MySQL, and hand logins over through Redis. The game server saves the
players into Redis, and a saver takes them on to MySQL (see [Saving players](#saving-players)).
Docker Compose runs MySQL, Redis and the saver, with the tables from `db/schema.sql`. The clients
talk to the servers over TLS, with certificates from an authority of our own, which
`tls/certificates.sh` makes the first time (see [TLS](#tls)):

```bash
docker compose up -d --build mysql redis saver
tls/certificates.sh
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
Each player is an animated character with a sword, on a board of tiles, with its level, name and
health over it, or a cube until you've made the characters (see below). Three skeletons wander
around the board. W, A, S and D, or the arrow keys, move yours, which has a white marker over it,
Shift makes it run, and Space swings the sword. The frame in the top left corner shows your health,
and your experience towards the next level.
View, Settings changes the background, and Game, Log out takes you back to the login screen. Next
time you log in, you're where you left off.

`protocol/nanommo.proto` is the contract between the client and the servers. The build runs protoc on
it, and all sides use the C++ it generates.

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.

## Characters

The players are Mixamo's X Bot and Y Bot, with an idle, a walk, a run, a sword slash and a death.
Mixamo's terms don't let us share its files, so they're not in the repo:
[assets/mixamo/README.md](assets/mixamo/README.md) lists what to download from mixamo.com. Then make
the characters:

```bash
docker build --target characters --output assets/characters .
```

The `characters` stage converts the FBX files to glTF with FBX2glTF, and
[assets/tools/merge.mjs](assets/tools/merge.mjs) puts each character and the clips into one `.glb`
that raylib can load: one skeleton, the walk and the run in place, and a tenth of the triangles. The
slash and the death keep their motion, so a player falls back as it dies. The build copies
`assets/characters` next to the client, which loads them from there. Without them, the players are
cubes, and the client's character tests skip themselves.

Each player holds Quaternius's sword, from [Poly Pizza](https://poly.pizza/m/ajOJ2NLz5m), which is
public domain and in the repo, as it was downloaded, in `assets/weapons`. The client scales it to a
metre and, each frame, puts it where the right hand's bone is in the pose: along the knuckles, with
the blade out past the index finger. glTF colours are linear, so the client turns the sword's into
sRGB, as our shader writes colours out as they are.

The server decides which way each player faces, and whether it stands, walks or runs. It moves them
at 1.8 and 3.1 metres a second, the speeds of the walk and the run, so that the feet don't slide.
The client blends from one clip into the next over a quarter of a second, and smooths each player's
position between the server's ticks.

## Monsters, health and levels

The monsters are Quaternius's skeleton, from [Poly Pizza](https://poly.pizza/m/wODZYCgX5Z). It's
public domain (CC0), so it's in the repo, in `assets/monsters`, as it was downloaded:
[assets/monsters/README.md](assets/monsters/README.md) says where it's from. The client loads it as
it is. It scales it to 1.6 metres, finds its clips by the word in their names, and walks with its
run, played at 0.4 of its speed, which puts the feet at the walking speed. It reads the bind pose
from the file's inverse bind matrices, as raylib binds the mesh to where the bones rest, and the
skeleton's arms rest about 70 degrees from where its mesh was bound.

The server spawns three skeletons, and runs their AI each tick: a monster rests for two to six
seconds, walks to a random place within three metres of where it spawned, at 1.8 metres a second,
and rests again. The world's random generator takes a seed, so the tests pick theirs.

## Fighting

Space swings your sword at the nearest monster within 1.6 metres, and turns you to face it. A swing
takes two seconds, the length of the slash animation, and you stand still until it's over. The
sword hits 0.9 seconds in, as it comes across the front, if the monster is still alive and in reach:
fifteen health at level one, and five more for each level after it. Holding Space down swings again
and again. All of it happens on the server: the client only says that Space is down, in `Input`.

A monster goes for the nearest living player within three metres of it, if that player is within
four metres of the monster's home. Each skeleton's home is further than that from the middle of the
board, so the middle, where players start and come back, is safe. A monster runs at them at the
players' running speed, and swings once it's within 1.2 metres. The skeleton's swing takes 0.9 seconds, its attack animation, and hits for five,
0.65 seconds in. Run, or die, and it gives up and wanders back.

A monster with no health left dies, and whoever killed it gets its experience, fifty for a skeleton.
Going up a level fills your health, to the new level's most. A player with no health left dies. The
dead play their death animation, lie there for ten seconds, and come back: a monster where it
spawned, and a player in the middle of the board, both at full health.

Players and monsters have a level and health, which everyone sees. A player's experience is its own:
the server works out its level, and sends its experience into the level, and what the next level
takes, to its client alone, in the message with its id that goes after the world, or with each part
over UDP. Each level takes a hundred more experience than the one before, and a player has a hundred
health at level one, and twenty more for each level after it.

Experience is saved with where you are (see [Saving players](#saving-players)). Health isn't saved
yet: you join at full health. A MySQL volume from before experience was added doesn't have the column, as
`db/schema.sql` only runs on an empty volume. `docker compose down -v` starts it again, without
the accounts.

## Accounts and logins

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it puts a token in Redis, which works once, within 30 seconds, and sends
  it to the client.
- The client takes the token to the game server, `nanommo_server`, on port 7777. The game server takes
  the token out of Redis, and loads your player: your name from the `accounts` table, and where you
  are and your experience from Redis, or from the `players` table if Redis doesn't have you.

The password and the token go over TLS, so nobody between the client and the servers can read
them (see [TLS](#tls)).

The servers find MySQL and Redis on `127.0.0.1`, or wherever `NANOMMO_MYSQL_HOST` and `NANOMMO_REDIS_HOST`
point. The tests that need them skip themselves unless those are set:

```bash
NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 ./build/bin/nanommo_server_tests
# Or all in Docker, next to MySQL and Redis.
docker compose run --rm tests
```

MySQL keeps its data in the `mysql` volume, and Redis in the `redis` volume, so they survive `docker
compose down`. MySQL runs `db/schema.sql` the first time it starts, on an empty volume. `docker
compose down -v` deletes the volumes, and with them all the accounts and players.

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
# The saver's image.
docker build --target saver -t nanommo-saver .
# API docs in site/.
docker build --target docs --output site .
```

In Docker, the client's tests open their window on a virtual display from Xvfb, and render with
llvmpipe, Mesa's OpenGL driver that runs on the CPU. So they don't need a screen or a GPU.

## TLS

Everything the clients and the servers send over TCP goes through TLS 1.3, with OpenSSL, through
Asio's `ssl::stream`: logins on port 7778, and the game's connection on port 7777, with the token,
the input and, over TCP, the world. Both ends are ours, so they speak TLS 1.3 and nothing older.

The certificates come from a certificate authority of our own. `tls/certificates.sh` makes its key,
`tls/ca.key`, and its certificate, `tls/ca.crt`. Then a key for the servers, `tls/server.key`, and a
certificate the authority signs for it, `tls/server.crt`, for `localhost`, `127.0.0.1`, and `login` and
`server`, their names in Docker Compose. The keys are ECDSA, on the P-256 curve. Names after the folder
go on the certificate too, such as a server's domain:

```bash
tls/certificates.sh tls play.example.com
```

It leaves certificates that are there already alone, so delete `tls/*.crt` and `tls/*.key` to make new
ones. Git ignores them, so each copy of the repo has an authority of its own.

- The servers load their certificate and key from `NANOMMO_TLS_CERTIFICATE` and `NANOMMO_TLS_KEY`
  (default: `tls/server.crt` and `tls/server.key`).
- The client and the bots trust the certificates the authority in `NANOMMO_TLS_CA` signed (default:
  `tls/ca.crt`), and no others, and check that the server's name is on the certificate.
- In Docker Compose, the `certificates` service makes them, the first time, before the servers start,
  and the servers and the bots get `tls/` read-only.

Only the authority's key can sign certificates the clients trust, so on a real server, keep it
somewhere else: the servers need `server.crt` and `server.key`, and the players' clients `ca.crt`.

The world over UDP isn't encrypted: it holds what each player sees in the game anyway. Its first
datagram, the hello, carries the login token, which the game server took out of Redis by then, so it
can't log anyone in. Someone who can see your traffic could use it to send your world updates
elsewhere, which someone in that position could stop anyway, by dropping them.

To see the handshake and the certificate, with the login server running:

```bash
openssl s_client -connect localhost:7778 -CAfile tls/ca.crt < /dev/null
```

## Saving players

After each tick, the game server saves the players whose position or experience changed, into Redis:
a hash for each player, `player:<account id>`, with `x`, `z` and `experience`, and their ids in a set,
`players:unsaved`. It sends all of it in one round trip, as a pipeline, so saving takes a few
milliseconds a tick with hundreds of players, and the server keeps up with as many players as it did
without saving. Writing each change to MySQL instead, one statement at a time, takes the tick past
its 50 ms at around 50 players: each save is two round trips, to prepare the statement and to run
it, and a commit that waits for the disk.

`nanommo_saver` takes the unsaved players out of the set every five seconds, and writes them to the
`players` table, 500 to an `INSERT`. If MySQL fails, they go back into the set, for the next try. A
player whose account was deleted, or whose hash is gone, is skipped, so that one player can't hold
back the others. It saves what's left when it stops. So the `players` table, and the website's
rankings, are at most about five seconds behind the game.

Redis appends each change to a file, its append-only file, and writes that to disk once a second, so
a crash loses at most about a second. If the game server crashes, Redis still has its players as of
the last tick. When a player joins, the game server reads them from Redis first, as it's newer than
MySQL, and from the `players` table if Redis doesn't have them.

```bash
docker compose exec redis redis-cli HGETALL player:1
docker compose exec redis redis-cli SCARD players:unsaved
docker compose logs saver
```

## The website

`web/` is the game's website, a [Laravel](https://laravel.com) app. It ranks the players by
experience, and you can create an account on it, log in and log out. It reads and writes the same
MySQL tables as the servers, so an account made on the website logs into the game, and the other way
round. It hashes passwords with Argon2id, with the login server's settings, in
`web/config/hashing.php`, so each reads the other's hashes. It keeps its sessions in Redis.

It runs in Docker, on [FrankenPHP](https://frankenphp.dev), a web server with PHP built in:

```bash
docker compose up -d --build web
```

Then open http://localhost:8080. [Pico CSS](https://picocss.com), from a CDN, styles the plain HTML,
so there's no CSS of our own, and nothing to build with npm.

- The rankings are the hundred players with the most experience. An account that has never played
  isn't on them. The saver brings them up to date every five seconds.
- Each address gets ten tries a minute at creating an account or logging in, since each try hashes
  a password, which takes 64 MiB and tens of milliseconds.
- The tables are still the ones from `db/schema.sql`. The website has no migrations of its own.
- `APP_KEY` in `compose.yaml` encrypts the website's cookies. Like the MySQL password next to it,
  it's for your own machine. Before the website goes anywhere public, make a new one with
  `docker compose run --rm --no-deps web php artisan key:generate --show`, and keep it out of git.

The website's tests, and Pint, Laravel's formatter, which fails on code it would change:

```bash
docker build --target test web
```

## Monitoring and load

The server serves its metrics for Prometheus at http://localhost:9464/metrics: the players online,
how long each tick takes, how long saving the players takes in each tick, the bytes it sends and the
messages it receives. The bots serve theirs on
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
- The players and the monsters go out in parts of up to 1,200 bytes, so that each fits in a packet and
  IP doesn't split it into fragments, which would all be lost with any one of them. A lost part leaves
  its players and monsters where they were for a tick. `protocol/parts.h` splits the world and
  puts it back together.
