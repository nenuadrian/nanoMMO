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

The servers keep accounts in MySQL, and hand logins over through Redis. The game server saves the
players into Redis, and a saver takes them on to MySQL (see [Saving players](docs/saving.md)).
Each game server is a world, which registers with Consul, and the login server lists the worlds
from there (see [Worlds](docs/worlds.md)). Docker Compose runs MySQL, Redis, Consul and the saver, and
before them Flyway, which makes the tables from the migrations in `db/` (see
[Migrations](docs/saving.md#migrations)). The clients talk to the servers over TLS, with certificates from an
authority of our own, which `tls/certificates.sh` makes the first time (see [TLS](docs/accounts.md#tls)):

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
connects to the world at the host and port that world registered, behind a loading screen until the
land is built (see [The loading screen](docs/graphics.md#the-loading-screen)). It gets the world over UDP, unless you put `tcp` after the
host (`./build/bin/nanommo_client localhost tcp`); see [Packet loss: TCP and UDP](docs/networking.md#packet-loss-tcp-and-udp).
Each player is an animated person, in their own skin and hair, and in a shirt of their own colour,
with a sword, on an island of hills, woods and beaches, with its level, name and health over it, or
a cube until you've made the characters (see [Characters](docs/graphics.md#characters)).
Skeletons wander around the middle, and in camps around the island, and a few NPCs stand near the
middle, each a person with its name over it (see [NPCs](docs/npcs.md)). Click the storyteller, in purple,
to ask it for a story (see [The storyteller](docs/npcs.md#the-storyteller)), the trainer, in grey, to learn
skills (see [Learning skills](docs/gameplay.md#learning-skills)), and the guard, with a gold mark over its name, for
a quest (see [Quests](docs/gameplay.md#quests)). W, A, S and D, or the arrow
keys, move yours, whose name is in gold, Shift makes it run, a click on a monster picks it, Space
strikes it with the sword, running up to it first, a double click on the ground walks you there, M
opens the map, and 1 to 4 use your skills, from the bar along the bottom (see [Skills](docs/gameplay.md#skills)). I, or View, Bag, opens your bag,
where a new player has three health potions, and wears a shirt, trousers and shoes (see
[Items](docs/gameplay.md#items)).
The camera follows you: Q and E, or dragging with the right mouse button, turn it around you,
dragging up and down tilts it, and the mouse wheel zooms in and out. W always walks away from the camera. The frame in the top left
corner shows your health, and your experience towards the next level. Enter opens the chat, in the
bottom left corner, where Enter again says what you typed to everyone in your world (see
[Chat](docs/gameplay.md#chat)).
Press C, or choose View, Character, to open your character screen. It shows your name, level,
health, experience and position, next to an idle preview of your character facing the camera, and
what you wear. The client renders that preview into a texture with a camera of its own, then ImGui
draws the texture in the window. View, Settings changes the sound, and the sky, which is the colour of the fog too, and Game, Log out
takes you back to the login screen. Next time you log in to that world, you're where you left off.

`protocol/nanommo.proto` is the contract between the client and the servers. The build runs protoc on
it, and all sides use the C++ it generates. Items, monsters and NPCs are data (see
[Content](docs/content.md)).

## Docs

How each part of the game works is in `docs/`, a page each, which the
[API docs](https://nenuadrian.github.io/nanoMMO/) have in their menu too:

- [Gameplay](docs/gameplay.md): monsters, health and levels, fighting, skills, items, quests and chat.
- [Content](docs/content.md): the items, monsters, NPCs, quests and skills, as data the servers, the client and the bots read.
- [NPCs](docs/npcs.md): who stands where, and the storyteller, which tells stories with mvGPT.
- [Graphics and sound](docs/graphics.md): the characters, the music, and the post-processing: bloom, cel shading and anti-aliasing.
- [The island](docs/island.md): the ground, the trees and the sea, made from a seed, and drawing them fast.
- [Worlds and lands](docs/worlds.md): the game servers, which Consul lists, and each world's lands and dungeons.
- [Networking](docs/networking.md): TCP and UDP when packets are lost, what each player gets, and what happens near them.
- [Accounts, logins and TLS](docs/accounts.md): the login server, the tokens, and TLS with an authority of our own.
- [Saving players and migrations](docs/saving.md): the players in Redis, the saver that takes them on to MySQL, and Flyway's migrations.
- [The website](docs/website.md): the Laravel website, with its rankings and accounts.
- [Monitoring, logs and analytics](docs/monitoring.md): Prometheus and Grafana, the bots, alerts, logs in Loki, and analytics in ClickHouse.
- [Benchmarking and profiling](docs/benchmarking.md): the load test, flame graphs, Pyroscope and Tracy.
- [Deployment](docs/deployment.md): a server with Docker Compose, handing players over without downtime, and deploying on push.
- [Code style and checks](docs/checks.md): clang-format and cpplint, the sanitizers, the fuzzers, clang-tidy and the coverage report.
