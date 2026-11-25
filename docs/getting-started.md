# Getting started

From a clone to playing, on macOS or Ubuntu 24.04.

| | macOS | Ubuntu 24.04 |
|---|---|---|
| Packages from | Homebrew | apt |
| Docker | Docker Desktop | Ubuntu's `docker.io`, with the Compose and buildx plugins |
| The first build | a few minutes | about four minutes on four cores |
| The first `docker compose up` | about ten minutes: it builds the saver's image | the same |
| Disk | about 1 GB for the build, and 10 GB for Docker's images | the same |

| Other systems | |
|---|---|
| Ubuntu 22.04 | Its CMake is 3.22 and its compilers are too old for C++23, and the game needs CMake 3.28 and GCC 14 |
| Windows | Builds with Visual Studio 2026 and vcpkg's libraries, and the tests pass, but CI doesn't check it; see [0.97.3](../CHANGELOG.md) for what it takes |
| Other Linux | Not tested |

## 1. Get the code

```bash
git clone https://github.com/nenuadrian/nanoMMO.git
cd nanoMMO
```

| To build | |
|---|---|
| The newest code | `main`, as cloned |
| An episode's code | `git checkout ep065/end`, or any episode's number |
| A version | `git checkout v0.65.0` |

## 2. Install what it needs

```bash
scripts/doctor.sh
```

It checks the tools, the libraries, Docker, the ports and the certificates, and ends with one command
that installs every missing package. What that is on a new machine:

| | Run |
|---|---|
| macOS | [Homebrew](https://brew.sh), `xcode-select --install`, `brew install cmake ninja pkgconf mysql-client libsodium openssl@3`, and [Docker Desktop](https://docs.docker.com/desktop/setup/install/mac-install/) |
| Ubuntu 24.04 | `sudo apt update && sudo apt install git cmake ninja-build build-essential g++-14 pkg-config libmysqlclient-dev libsodium-dev libssl-dev xorg-dev libgl1-mesa-dev docker.io docker-compose-v2 docker-buildx` |
| Ubuntu 24.04's compiler | Its GCC 13 is too old for the C++23 the game uses, so build with GCC 14: `echo 'export CXX=g++-14' >> ~/.bashrc`, then open a new terminal |
| Ubuntu, once | `sudo usermod -aG docker $USER`, then log out and in again |

Run `scripts/doctor.sh` again until every line is a tick.

## 3. Build and test

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The release preset builds into `build/`. `cmake --list-presets` lists the others, the sanitizers and
the rest of [the checks](checks.md), each in a folder of its own.

| Where | Tests |
|---|---|
| With a screen | `ctest --preset release` |
| Without one, such as over SSH | `sudo apt install xvfb`, then `xvfb-run -a ctest --preset release` |
| With the database tests, once step 4's services are up | `NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 NANOMMO_CONSUL_HOST=127.0.0.1 ctest --preset release` |

| You see | It means |
|---|---|
| The first build takes minutes | CMake fetches the libraries a system doesn't have, and builds them: GoogleTest, raylib, Dear ImGui, rlImGui, abseil, protobuf, Asio, prometheus-cpp, hiredis, nlohmann/json and cpp-httplib |
| Warnings from `_deps/` | They're the libraries', built as they come: the game's own code builds without one |
| `100% tests passed`, and some `(Skipped)` | It's built. The skipped tests need MySQL, Redis and Consul, as in the last row above |
| `Can't connect to local MySQL server through socket` | MySQL's library takes `localhost` to mean its socket file: use `127.0.0.1` |

## 4. Start the servers

```bash
docker compose up -d --build mysql redis consul saver
scripts/certificates.sh
./build/bin/nanommo_login
```

Then, in another terminal:

```bash
./build/bin/nanommo_server
```

| Runs | What it is | Port |
|---|---|---|
| `docker compose up` | MySQL, Redis, Consul, and the saver that takes players from Redis to MySQL | 3306, 6379, 8500 |
| `scripts/certificates.sh` | The certificates, once: [TLS](accounts.md#tls) | |
| `nanommo_login` | The login server | 7778 |
| `nanommo_server` | A world, Ember | 7777 TCP and UDP, 9464 for metrics |

| To also have | Run |
|---|---|
| The storyteller's stories | `docker compose up -d --build storyteller`: [The storyteller](npcs.md#the-storyteller) |
| A second world | `NANOMMO_WORLD=Frost NANOMMO_METRICS_PORT=9465 ./build/bin/nanommo_server 7787`: [Worlds](worlds.md) |
| Ten bots, playing | `./build/bin/nanommo_bot localhost localhost 10 10`: [Monitoring](monitoring.md) |
| Playing in a browser, online | `./build/bin/nanommo_gateway`, on port 8090, and the web build: [The client in a browser](web.md#online-through-the-gateway) |

## 5. Play

```bash
./build/bin/nanommo_client
```

| Screen | Do |
|---|---|
| Login | A name and a password: Make an account the first time, Log in after |
| Worlds | Ember, with the arrow keys and Enter, or a click |
| Characters | New character, or straight there with nobody yet: a name, a build, a skin and hair, and Create. Then Enter the world |

| Key | Does |
|---|---|
| W A S D, or the arrows | Walk, away from the camera |
| Shift | Run |
| A click on a monster | Picks it |
| Space | Strikes the monster picked, running up to it first |
| 1 to 8 | The skills on the bar along the bottom |
| A double click on the ground | Walks there |
| Q and E, or dragging with the right button | Turn the camera |
| The mouse wheel | Zooms |
| M | The map |
| I | The bag |
| C | The character |
| T | Your quests |
| G | The guild |
| K | Crafting |
| H | The help, with these keys and more |
| O | The settings |
| Enter | The chat, and `/g` and a space before a line for your guild |
| Escape | Closes what's open, or opens the menu, with Settings and Log out |

More in [Gameplay](gameplay.md).

Without the servers, `./build/bin/nanommo_client --offline` plays a world of the client's own: the
Meadows, for you alone, with nothing saved, as a browser plays it ([Offline](web.md#offline)).

## 6. Play from another computer

On the computer with the servers, at `192.168.1.20` in this example:

```bash
scripts/certificates.sh tls 192.168.1.20
./build/bin/nanommo_login
NANOMMO_PUBLIC_HOST=192.168.1.20 ./build/bin/nanommo_server
```

On the other computer, with the game built at the same version:

```bash
scp you@192.168.1.20:nanoMMO/tls/ca.crt tls/their-ca.crt
NANOMMO_TLS_CA=tls/their-ca.crt ./build/bin/nanommo_client 192.168.1.20
```

| Why | |
|---|---|
| `scripts/certificates.sh tls 192.168.1.20` | The certificate has to name the address the client asks for |
| `NANOMMO_PUBLIC_HOST` | A world tells players where to find it, and it's `localhost` without this |
| `tls/their-ca.crt` | The client trusts only the authority that signed the servers' certificate |
| The same version | The login server turns away a client of another minor version |
| A firewall | Let in 7778 over TCP and 7777 over TCP and UDP: `sudo ufw allow 7778/tcp && sudo ufw allow 7777` |
| macOS | Give the terminal Local Network access: System Settings, Privacy & Security, Local Network |

## 7. Stop and start over

| To | Run |
|---|---|
| Stop a server | Ctrl-C in its terminal |
| Stop the services | `docker compose down` |
| Start over, without accounts or players | `docker compose down -v` |
| See the saver's log | `docker compose logs saver` |
| Find who holds a port | `scripts/doctor.sh` |

## When something goes wrong

| You see | Why | Do |
|---|---|---|
| `cmake: command not found`, or `CMake 3.28 or higher is required` | No CMake, or an old one | `scripts/doctor.sh` says which, and what to install |
| `too old for C++23` from `scripts/doctor.sh`, or errors at `this auto self` | A compiler from before the parts of C++23 the game uses, such as Ubuntu 24.04's GCC 13 | GCC 14, as in step 2. CMake keeps the compiler of its first run, so `rm -rf build` before building again |
| `permission denied while trying to connect to the Docker daemon socket` | Docker is the docker group's | `sudo usermod -aG docker $USER`, then log out and in again |
| `the --mount option requires BuildKit` | No buildx | `sudo apt install docker-buildx` |
| `could not determine a constructor for the tag '!override'` | Compose 1, from `apt install docker-compose` | `sudo apt install docker-compose-v2`, and `docker compose`, with a space |
| `address already in use`, or `port is already allocated` | Something else holds one of the ports | `scripts/doctor.sh` says who |
| Renderer, Sea and other client tests fail, and `DISPLAY` is missing | No screen | `xvfb-run -a ctest --preset release` |
| `No worlds are up.` | No world has registered with Consul | Start `nanommo_server`, then Refresh |
| `its certificate doesn't name 192.168.1.20` | The certificate lacks the address you asked for | `scripts/certificates.sh tls 192.168.1.20` on the server, then start the servers again |
| `its certificate has expired` | The servers' certificate lasts 825 days | `scripts/certificates.sh` on the server, then start the servers again |
| `its certificate isn't from the authority we trust` | The client has another copy's `ca.crt` | Copy the server's `tls/ca.crt`, and point `NANOMMO_TLS_CA` at it |
| `Ember says it's at localhost` | The world was started without its address | `NANOMMO_PUBLIC_HOST=<its address> ./build/bin/nanommo_server` |
| `couldn't serve metrics on port 9464` | Another world runs on this machine | Give this one its own port and `NANOMMO_METRICS_PORT` |
| `This client is 0.64.0, and the servers are 0.65.0` | The client and the servers were built at different versions | Build both at the same tag |
| `No route to host`, on macOS | The terminal may not use the local network | System Settings, Privacy & Security, Local Network |
| The storyteller says it can't think of a story | It isn't running | `docker compose up -d --build storyteller` |
| `couldn't load ... background.mp3` | The music isn't in git, for its licence | The game plays without it: [assets/music](../assets/music/README.md) |
