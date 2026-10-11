# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler. The client draws with raylib, on top of the system's
OpenGL:

- macOS and Windows: nothing more to install.
- Ubuntu: `sudo apt install xorg-dev libgl1-mesa-dev`, the X11 and OpenGL headers.

CMake fetches and builds the libraries: GoogleTest, raylib, Dear ImGui, rlImGui, abseil, protobuf,
Asio and prometheus-cpp. The first build compiles protobuf, so it takes a few minutes.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/bin/nanommo_server
```

Then, in another terminal or two:

```bash
./build/bin/nanommo_client
```

The server listens on TCP port 7777, and the client connects to `localhost`, or to the host you give
it (`./build/bin/nanommo_client 192.168.1.20`). Each player is a cube on a board of tiles. W, A, S and D,
or the arrow keys, move yours, which has a white marker over it. View, Settings changes the
background.

`protocol/nanommo.proto` is the contract between the client and the server. The build runs protoc on it,
and both sides use the C++ it generates.

Or with only Docker installed:

```bash
# Build and run the tests on Ubuntu.
docker build --target test .
# Check the code style.
docker build --target lint .
# The server image, if the tests pass.
docker build --target server -t nanommo-server .
# Run the server, on port 7777, with its metrics on 9464.
docker run --rm -p 7777:7777 -p 9464:9464 nanommo-server
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
how long each tick takes, the bytes it sends and the messages it receives.

`compose.yaml` starts the server together with Prometheus, which scrapes it every two seconds, and
Grafana, with a dashboard of those metrics. The dashboard and Grafana's data source are files in
`monitoring/`, so they're set up the same way on any machine.

```bash
docker compose up -d --build
```

- Grafana: http://localhost:3000, which opens on the nanoMMO server dashboard.
- Prometheus: http://localhost:9090.

`nanommo_bot` connects a swarm of bots that wander around like players at their keys. It takes the
host, the port, how many bots, and how many seconds to spread their arrival over:

```bash
./build/bin/nanommo_bot localhost 7777 100 10
```

Or in Docker, next to the server: 300 bots, arriving over two minutes.

```bash
docker compose up -d bots
```

A few hundred bots need more open files than macOS allows a shell by default, so on the Mac, run
`ulimit -n 4096` first. Stop everything with `docker compose --profile load down`.
