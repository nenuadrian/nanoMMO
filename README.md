# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler. The client draws with raylib, which CMake fetches and
builds, on top of the system's OpenGL:

- macOS and Windows: nothing more to install.
- Ubuntu: `sudo apt install xorg-dev libgl1-mesa-dev`, the X11 and OpenGL headers.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/bin/nanommo_server
./build/bin/nanommo_client
```

The client opens a window with a menu bar along the top, over a cube on a board of tiles. W, A,
S and D, or the arrow keys, move the cube, and View, Settings changes the background.

Or with only Docker installed:

```bash
docker build --target test .                  # build and run the tests on Ubuntu
docker build --target lint .                  # check the code style
docker build --target server -t nanommo-server .  # the server image, if the tests pass
docker run --rm nanommo-server                    # run the server
docker build --target docs --output site .    # API docs in site/
```

In Docker, the client's tests open their window on a virtual display from Xvfb, and render with
llvmpipe, Mesa's OpenGL driver that runs on the CPU. So they don't need a screen or a GPU.

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.
