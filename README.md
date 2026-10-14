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
protobuf, Asio, prometheus-cpp, hiredis, nlohmann/json and cpp-httplib. The first build compiles protobuf, so it takes a few
minutes.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The servers keep accounts in MySQL, and hand logins over through Redis. The game server saves the
players into Redis, and a saver takes them on to MySQL (see [Saving players](#saving-players)).
Each game server is a world, which registers with Consul, and the login server lists the worlds
from there (see [Worlds](#worlds)). Docker Compose runs MySQL, Redis, Consul and the saver, and
before them Flyway, which makes the tables from the migrations in `db/` (see
[Migrations](#migrations)). The clients talk to the servers over TLS, with certificates from an
authority of our own, which `tls/certificates.sh` makes the first time (see [TLS](#tls)):

```bash
docker compose up -d --build mysql redis consul saver
tls/certificates.sh
./build/bin/nanommo_login
```

Then, in another terminal, the game server, and in one or two more, the client:

```bash
./build/bin/nanommo_server
./build/bin/nanommo_client
```

The client opens on the worlds that are up, with how many players each has. Pick one with the arrow
keys and Enter, or a click, and F5 or Refresh asks for the list again. Then type a name and a
password, and Create account the first time, and Log in after that. The client asks the login
server on `localhost`, or on the host you give it (`./build/bin/nanommo_client 192.168.1.20`), and
connects to the world at the host and port that world registered. It gets the world over UDP, unless you put `tcp` after the
host (`./build/bin/nanommo_client localhost tcp`); see [Packet loss: TCP and UDP](#packet-loss-tcp-and-udp).
Each player is an animated character with a sword, on an island of hills, woods and beaches, with
its level, name and health over it, or a cube until you've made the characters (see below).
Skeletons wander around the middle, and in camps around the island. W, A, S and D, or the arrow
keys, move yours, which has a white marker over it, Shift makes it run, and Space swings the sword.
The camera follows you: Q and E, or dragging with the right mouse button, turn it around you, and
the mouse wheel zooms in and out. W always walks away from the camera. The frame in the top left
corner shows your health, and your experience towards the next level.
Press C, or choose View, Character, to open your character screen. It shows your name, level,
health, experience and position, next to an idle preview of your character facing the camera. The
client renders that preview into a texture with a camera of its own, then ImGui draws the texture in
the window. View, Settings changes the sky, which is the colour of the fog too, and Game, Log out
takes you back to the login screen. Next time you log in to that world, you're where you left off.

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

## The world

Each land's island comes from a seed, a number: the game server's `NANOMMO_WORLD_SEED`, 1 by
default, plus the land's own (see [Lands](#lands)). The server sends it to each client with the
client's own id, and both make the same island from it, in `terrain/`, so the island itself never
goes over the network.

- The ground is a grid of cells two metres across, 256 of them along each side, so 512 metres, with
  two triangles in each cell. Each corner's height is value noise: a random number at each whole
  number, from a hash of the seed and the point, and smooth between them. Five octaves of it, each
  twice as fine and half as strong as the one before, make the hills, and four more, larger, make
  the mountains. Towards the edge, the land sinks into the sea, and in the middle, where the players
  start, it's gentle and above the sea.
- A player stands on the triangles, as the client draws them, not on the noise between the corners.
- The trees grow on a grid of four-metre squares, at most one in each, between the beach and the
  rock, and not within sixteen metres of the middle: in woods where the noise is high, and a few on
  their own elsewhere.
  Each square's hash says where in the square its tree stands, which way it faces and how big it is.
- The server lets players and monsters walk anywhere on the island, and into the sea up to their
  knees, 40 centimetres, but not through a trunk. Running into the sea or a tree, they slide along
  whichever way they still can. A player saved somewhere that's sea now starts in the middle.

The server and the clients have to make the same heights and trees to the last bit: the server
decides where the trees are in the way, and the client draws them. Compilers for Arm, and for x86
with `-march` options that have FMA, fuse a multiply and an add into one instruction, which rounds
once instead of twice, so the same code would make a slightly different island on a Mac than on an
x86 server. `terrain/CMakeLists.txt` turns that off with `-ffp-contract=off`, and the noise uses
no `std::sin` or `std::pow`, whose results differ between standard libraries. The terrain test pins a
fingerprint of the whole island, its heights and its trees, so a machine that makes another island
fails it.

The client colours each face by its height and slope: sand on the beaches, grass, rock on steep
slopes and the high ground, and snow on the peaks, each face a little lighter or darker than its
neighbours. The triangles share no corners, so each has the normal of its face, and the light shades
each face flat. The sea is one square, at sea level, the island's colour showing through it near
the shore.

The colours are the land's look, in `client/atmosphere.h`: those of the ground, the sea and the
trees, which go into the meshes, and an atmosphere, which the shaders read each frame. The
atmosphere is the sky, which the fog fades into, where the fog starts and where it hides everything,
which way the sun is, the light it adds to a face turned to it, and the light a face gets even when
it's turned away. In the Meadows, a warm sun shines from the side and low, so the faces of the hills
and trees stand apart, and the fog goes from 45 metres to 150. The shaders work out the fog for each
pixel, from how far it is, so a face as big as the sea fades too. View, Settings changes the
atmosphere as you play, with presets for dusk, night, mist and ash, and Reset puts the land's back.

The tree is Poly by Google's, from [Poly Pizza](https://poly.pizza/m/6pwiq7hSrHr), under
[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/), in the repo, as it was downloaded, in
`assets/trees` (see [assets/trees/README.md](assets/trees/README.md)). Its texture is a palette of
four colours, so the client reads the model as triangles, and gives each face the colour under its
middle.

### Drawing the island fast

The client cuts the island into chunks, squares of 32 metres, and puts each chunk's ground and trees
into one mesh, with the trees moved into place. Each frame, it draws the chunks that are closer than
where the fog hides everything, 150 metres in the Meadows, and inside the camera's view, and no others. A chunk is out of view when all
eight corners of its box are past the same side of the screen, or nearer than the near plane, or
past the far one.

`nanommo_client_bench` measures it. It walks once around the middle of the island, 80 metres out,
with the camera turning to look over the player at the middle, drawing the same frames each time,
as fast as it can, and then prints the frames per second, the median and 99th percentile frame time,
and the draws and triangles each frame. It takes the number of frames, 1200 by default, the seed,
and a file to write the numbers to as JSON too:

```bash
./build/bin/nanommo_client_bench
./build/bin/nanommo_client_bench 1200 1 bench/results/frames.json
```

Build it as Release (`-DCMAKE_BUILD_TYPE=Release`) to measure. With a draw for each tree and the
ground as one mesh, it took 1518 draws and 369,086 triangles a frame, and 1.7 ms a frame at the
median on an M4 Pro. With the chunks, it's 20 draws and about 62,000 triangles, and 0.25 ms. In
Docker, with Mesa's llvmpipe drawing on the CPU, it went from 14.8 ms to 13.0 ms: on the CPU, most
of the time goes on the pixels, not the triangles.

```bash
docker build --target build -t nanommo-build .
docker run --rm nanommo-build xvfb-run --auto-servernum -s "-screen 0 1280x720x24" \
  ./build/bin/nanommo_client_bench 300
```

## Monsters, health and levels

The monsters are Quaternius's skeleton, from [Poly Pizza](https://poly.pizza/m/wODZYCgX5Z). It's
public domain (CC0), so it's in the repo, in `assets/monsters`, as it was downloaded:
[assets/monsters/README.md](assets/monsters/README.md) says where it's from. The client loads it as
it is. It scales it to 1.6 metres, finds its clips by the word in their names, and walks with its
run, played at 0.4 of its speed, which puts the feet at the walking speed. It reads the bind pose
from the file's inverse bind matrices, as raylib binds the mesh to where the bones rest, and the
skeleton's arms rest about 70 degrees from where its mesh was bound.

The server spawns three skeletons around the middle of the island, and up to three more at each of
eight camps, which the world's seed places between the beach and the rock, at least thirty metres
from the middle. It runs their AI each tick: a monster rests for two to six seconds, walks to a
random place within three metres of where it spawned, at 1.8 metres a second, and rests again. If
the sea or a tree stops it, it rests, and picks another place. The world's random generator takes a
seed, so the tests pick theirs.

## Fighting

Space swings your sword at the nearest monster within 1.6 metres, and turns you to face it. A swing
takes two seconds, the length of the slash animation, and you stand still until it's over. The
sword hits 0.9 seconds in, as it comes across the front, if the monster is still alive and in reach:
fifteen health at level one, and five more for each level after it. Holding Space down swings again
and again. All of it happens on the server: the client only says that Space is down, in `Input`.

A monster goes for the nearest living player within three metres of it, if that player is within
four metres of the monster's home. Each skeleton's home is further than that from the middle of the
island, so the middle, where players start and come back, is safe. A monster runs at them at the
players' running speed, and swings once it's within 1.2 metres. The skeleton's swing takes 0.9 seconds, its attack animation, and hits for five,
0.65 seconds in. Run, or die, and it gives up and wanders back.

A monster with no health left dies, and whoever killed it gets its experience, fifty for a skeleton.
Going up a level fills your health, to the new level's most. A player with no health left dies. The
dead play their death animation, lie there for ten seconds, and come back: a monster where it
spawned, and a player in the middle of the island, both at full health.

Players and monsters have a level and health, which everyone sees. A player's experience is its own:
the server works out its level, and sends its experience into the level, and what the next level
takes, to its client alone, in the message with its id that goes after the world, or with each part
over UDP. Each level takes a hundred more experience than the one before, and a player has a hundred
health at level one, and twenty more for each level after it.

Experience and health are saved with where you are (see [Saving players](#saving-players)), so you
come back with the health you left with. If you leave dead, you come back as the respawn would bring
you back, in the middle at full health. A player saved before health was kept joins at full health.

## Accounts and logins

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it puts a token in Redis, which works once, within 30 seconds, and sends
  it to the client.
- The client takes the token to the world you picked, a game server, `nanommo_server`. The game
  server takes the token out of Redis, and loads your player in that world: your name from the
  `accounts` table, and where you are and your experience from Redis, or from the `players` table if
  Redis doesn't have you.

The password and the token go over TLS, so nobody between the client and the servers can read
them (see [TLS](#tls)).

The servers find MySQL, Redis and Consul on `127.0.0.1`, or wherever `NANOMMO_MYSQL_HOST`,
`NANOMMO_REDIS_HOST` and `NANOMMO_CONSUL_HOST` point. The tests that need them skip themselves
unless those are set:

```bash
NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 NANOMMO_CONSUL_HOST=127.0.0.1 \
  ./build/bin/nanommo_server_tests
# Or all in Docker, next to MySQL and Redis.
docker compose run --rm tests
```

MySQL keeps its data in the `mysql` volume, and Redis in the `redis` volume, so they survive `docker
compose down`. Flyway makes the tables, and changes them, before anything else uses MySQL (see
[Migrations](#migrations)). `docker compose down -v` deletes the volumes, and with them all the
accounts and players.

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
go on the certificate too, such as a server's domain, or its address, which goes on as an IP name:

```bash
tls/certificates.sh tls play.example.com 203.0.113.7
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

After each tick, the game server saves the players whose position, experience or health changed, into
Redis: a hash for each player in each world, `player:<world>:<account id>`, with `world`, `account`,
`x`, `z`, `experience`, `health` and `land`, and its key in a set, `players:unsaved` (see [Worlds](#worlds)). It sends all of it in one round trip, as a pipeline, so saving takes a few
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
docker compose exec redis redis-cli HGETALL player:Ember:1
docker compose exec redis redis-cli SCARD players:unsaved
docker compose logs saver
```

## Migrations

The tables are made and changed by migrations, SQL files in `db/` that [Flyway](https://github.com/flyway/flyway)
runs from its official Docker image. Docker Compose runs it as the `flyway` service, and the login
server, the game server, the saver, the website and the tests wait until it has finished. If it
fails, they don't start.

- Each file is named `V<version>__<description>.sql`, with two underscores, such as
  `V2__players_health.sql`. Flyway runs the versions a database hasn't run yet, in order, and records
  each one in the `flyway_schema_history` table, with a checksum of its file. A file whose name it
  can't read stops it, instead of being skipped.
- Don't change a migration once it has run anywhere. Flyway compares the checksums and refuses to go
  on, as a database that ran the old file wouldn't get the change. A change goes in a new file, with
  the next version.
- Write each change so that the servers from before it still work, as they keep running until the
  new ones replace them: add a column that can be `NULL` or has a default, and drop or rename
  things in a later version, once no code uses them. `ALGORITHM=INSTANT` makes MySQL refuse a change
  it can't make by changing the table's definition alone, instead of quietly rebuilding the table,
  which takes a long time on a big one.
- On a server with real players, back the database up before running new migrations.

```bash
docker compose run --rm flyway info      # which versions have run
docker compose run --rm flyway migrate   # run the new ones
```

A MySQL volume from before the migrations has the tables but no history, and Flyway won't touch it.
`docker compose down -v` starts again from an empty one, without the accounts in it. On a database
you want to keep, Flyway's `baseline` records version 1 without running it.

Flyway 13.10 warns that it has been verified with MySQL up to 9.4, and we run 9.7. Our migrations are
plain SQL, and CI runs them on a fresh database on each push. Its URL turns TLS on, as MySQL's default
authentication, `caching_sha2_password`, takes a password over an encrypted connection, or with
MySQL's RSA key, and it trusts the certificate MySQL makes for itself, which is fine on Compose's own
network.

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
- The tables come from the migrations in `db/`, which Flyway runs (see [Migrations](#migrations)).
  The website has no migrations of its own, so the tables are described in one place.
- `APP_KEY` in `compose.yaml` encrypts the website's cookies. Like the MySQL password next to it,
  it's for your own machine. On a server, `deploy.sh` makes a new one (see [Deployment](#deployment)).
  To make one yourself, run `docker compose run --rm --no-deps web php artisan key:generate --show`,
  and keep it out of git.

The website's tests, and Pint, Laravel's formatter, which fails on code it would change:

```bash
docker build --target test web
```

## Monitoring and load

Each world serves its metrics for Prometheus at http://localhost:9464/metrics: the players online in each land,
how long each tick takes, how long saving the players takes in each tick, the bytes it sends and the
messages it receives. The bots serve theirs on
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

`nanommo_bot` connects a swarm of bots that play like players. Each one runs to a random place on the
island, within 170 metres of the middle, or on one trip in five into its land's gate, and wanders around there like a player at the keys, running a
third of the time, for 5 to 15 directions of half a second to 3 seconds each, before it sets off
again. A bot that stops
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
of the players. Now it serializes each player and monster once a tick, and the sessions write those
bytes, followed by each client's own id. Most of the tick goes on the kernel sending the bytes, packet
by packet, which is why each player gets what's near them, and no more (see below).

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

## What each player gets

Each tick, the server sends each player the players and monsters near them, which is called interest
management. Sending everyone the whole world grows with the square of the players, and most of it
would be hidden in the fog anyway.

- `server/interest.h` sorts the players and monsters into cells of 32 by 32 metres. A player gets
  their own cell and the two cells each way around it, so everyone within 64 metres, and some further
  out. A player who comes into view does that past the nameplates, which show within 50 metres, and
  partly in the fog. `kInterestCells` sets how far that is.
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

## Worlds

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

- A world registers with its name, its address, the public host and port, the metrics port and how
  many players it has, and a health check that turns critical ten seconds after its last pass.
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
- A world's name is its ID, in Consul and in the `players` table, so two worlds with one name would
  share their players, and the second to register would replace the first in the list.
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

Each world has two lands, each an island of its own: the Meadows, where players start, and the
Ashlands, where ash and charred trees stand around lava, under a red haze. The world server runs both
in its one tick loop, each as a `World`, with its own players and monsters. `terrain/lands.h` lists
them for the server, the client and the bots, in one order, as the server tells a client which land
it's in by its place in the list:

| Land | Seed | Gate |
|---|---|---|
| Meadows | the world's seed | 10 metres north of the middle, to the Ashlands |
| Ashlands | the world's seed + 1000 | 10 metres north of the middle, to the Meadows |

- Players wade into the Meadows' sea up to their knees, and not into the Ashlands' lava: each land
  says how deep its sea can be waded, and its `Terrain` takes that.
- A gate is a ring of standing stones around a slab in the colour of the other land's sky. A player
  who walks within a metre of its middle goes to the other land, and comes out three metres past the
  middle of that land's gate, the way they were facing, so that they walk on out of it.
- The server sends each client its land and its land's seed, and the client builds that land in its
  look, from `client/atmosphere.h`, whenever the seed changes. That takes a moment, as it makes the
  island and its chunks again.
- The `players` table and the players' hashes in Redis keep the land a player is in, by its name, so
  they come back where they left. The migration gives the column the Meadows as its default, which is
  where the players from before were, and keeps the servers from before saving while a deploy
  migrates.
- The players-online metric has a `land` label, and the dashboard shows each world's lands apart.
- The bots go into their land's gate on one trip in five, so they spread over both lands.

Running a world's lands in one process makes a trip through a gate a move from one `World` to
another, within a tick. With a server for each land, each land could have a machine of its own, and
a trip would hand the player over from one server to the other, with a save and a token, like a
login.

## Deployment

The game runs on any Linux server with Docker and Docker Compose, such as a VPS, from
`compose.prod.yaml` on top of `compose.yaml`. On the server, `.env` sets
`COMPOSE_FILE=compose.yaml:compose.prod.yaml`, so each `docker compose` command there reads both.

- The internet reaches the login server on TCP port 7778, the worlds on ports 7777 and 7787, TCP and
  UDP, and the website on port 8080, and nothing else. MySQL, Redis and the worlds' metrics stay on
  Compose's own network. Docker publishes a port past a firewall such as ufw, so leaving a port out of
  the file is what keeps it off the internet.
- Prometheus, Grafana and Consul listen on the server's localhost. Reach them through SSH:
  `ssh -L 3000:localhost:3000 -L 9090:localhost:9090 -L 8500:localhost:8500 debian@203.0.113.7`,
  then open http://localhost:3000.
- MySQL's password and the website's `APP_KEY` come from `.env` on the server, which `deploy.sh` makes
  on the first deploy, with random ones, readable by the deploying user alone. Git and the Docker
  builds leave `.env` out.
- The worlds' public host comes from `.env` too, as `NANOMMO_PUBLIC_HOST`: `deploy.sh` adds it, from
  the address it deploys to, when `.env` doesn't have it yet.
- The services start again after a crash or a reboot (`restart: unless-stopped`).

The certificates come from an authority of your own, made on your machine, with the server's address
on them. `deploy.sh` sends the servers their certificate and key, and the authority's key stays with
you. The clients and the bots trust the authority's certificate:

```bash
tls/certificates.sh tls/vps 203.0.113.7                 # once
./deploy.sh debian@203.0.113.7 tls/vps                  # this commit, or name one: ... tls/vps ep019/end
NANOMMO_TLS_CA=tls/vps/ca.crt ./build/bin/nanommo_client 203.0.113.7
```

`deploy.sh` sends the commit with `git archive`, so the server needs no git or access to the repo,
with the certificates added under `tls/`, all in one SSH connection, so that a deploy right after
other SSH commands isn't refused: ufw's `limit` on SSH refuses a sixth connection within 30 seconds.
The server unpacks it into `nanommo.next`, keeps `.env`, and builds the images there first. The
tests run as part of the build, so a release that fails them, or doesn't build, leaves the running
release as it is. Then the release before moves into `nanommo.previous`, the new one into `nanommo`,
and `docker compose up -d --wait --force-recreate` starts each container again from the new
release's folder: one left running would keep the folder it started from. The images build on the
server from the source, protobuf and all: about fifteen minutes on 2 vCPUs, the first time and
each time after. To go back to the release before, which builds its images again:

```bash
ssh debian@203.0.113.7 'rm -rf nanommo && mv nanommo.previous nanommo && cd nanommo && docker compose up -d --build --wait --force-recreate'
```

The volumes, with the accounts and the players, belong to the Compose project, not to a release, so
they stay through each deploy, and Flyway runs any new migrations before the servers start. Going
back leaves the database as the newer release left it, as Flyway skips migrations it doesn't know,
so a migration has to keep the release before it working. New
certificates need the servers to start again: `ssh debian@203.0.113.7 'cd nanommo && docker compose
restart login server frost'`.

The benchmark runs on the server too, with the bots there, as `deploy.sh` sends the authority's
certificate along for them. It needs `jq` on the server (`sudo apt install jq`), and `curl`, which
Debian has:

```bash
ssh debian@203.0.113.7 'cd nanommo && bench/load.sh 100 10' > bench/results/vps.json
```

On a VPS with 2 vCPUs, which the bots share with the servers, the game server keeps its 20 ticks a
second up to about 700 players (see What each player gets).
