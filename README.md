# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler. The client draws with raylib, on top of the system's
OpenGL:

- macOS and Windows: nothing more to install.
- Ubuntu: `sudo apt install xorg-dev libgl1-mesa-dev`, the X11 and OpenGL headers.

CMake fetches and builds the libraries: GoogleTest, raylib, Dear ImGui, rlImGui, abseil, protobuf
and Asio. The first build compiles protobuf, so it takes a few minutes.

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
# Run the server, on port 7777.
docker run --rm -p 7777:7777 nanommo-server
# API docs in site/.
docker build --target docs --output site .
```

In Docker, the client's tests open their window on a virtual display from Xvfb, and render with
llvmpipe, Mesa's OpenGL driver that runs on the CPU. So they don't need a screen or a GPU.

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.
