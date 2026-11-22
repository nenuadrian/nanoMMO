# The client in a browser

The client builds to WebAssembly with Emscripten, and draws in a page with WebGL 2. The same C++
runs in both: what a browser can't do goes behind a small seam, and a few settings differ. A
page can't reach the servers, so it plays offline, in a world of its own.

## Build it

It takes Emscripten, which it was built with at 7.0 (`brew install emscripten` on macOS, or the
[emsdk](https://emscripten.org/docs/getting_started/downloads.html) elsewhere), and the release
build first, whose `protoc` the web build runs:

```bash
cmake --preset release && cmake --build --preset release
emcmake cmake --preset web
cmake --build --preset web
cd build-web/bin && python3 -m http.server 8000
```

Then open `http://localhost:8000/nanommo_client.html`. A page has to come from a server, not a
file, as the browser won't let a `file://` page load the files next to it.

## Offline

Without the servers, the client runs a world itself: `LocalConnection`, in
`client/local_connection.h`, is a `Link` like `Connection`, with the world's library
(`nanommo_world`, in [Worlds](worlds.md#the-worlds-library)) behind it in place of a socket. It
steps the world at the server's tick, 20 times a second, and what the player does and what they're
shown go through `Inputs`, `DoItem` and `server/shown.h`, as on a world server, so the client draws
it as it draws a server's world and nothing of the server's is copied.

A page opened without a gateway (below) plays offline from the start, as `CanReachServers()` is
false there. On a computer, `./build/bin/nanommo_client --offline` does the same, without the
servers or a certificate.

| Offline, there's | Not there, as it lives on the servers |
|---|---|
| The Meadows, with its monsters and NPCs, and one player, Traveller | Other players, and the other lands: a gate says so in the chat |
| Fighting, skills, levels, the bag and its gold, and the NPCs' skills and quests | The storyteller's stories, guilds, and trades and duels, which take another player |
| Everything the client draws and plays | Saving: a page left or closed starts again |

## Online, through the gateway

A page can't open a TCP or UDP socket, but it can open a WebSocket, so a page reaches the servers
through the gateway, `nanommo_gateway` (`gateway/`), which Compose runs on port 8090. It takes a
page's WebSocket and connects it to a server over TLS, as a client on a computer connects, and
carries the game's messages both ways as they are, so the servers don't change:

| The page opens | The gateway connects it to |
|---|---|
| `/login` | The login server, at `NANOMMO_LOGIN_HOST`, on port 7778 |
| `/world/<host>/<port>` | The world Consul lists at that host and port, the ones the login server lists, at its own address on Consul's network, with its certificate checked by the host's name |

Anything else, and a world Consul doesn't list there, is turned down, so the gateway reaches our
servers and nothing else. It asks Consul for the worlds once a second, on a thread of its own, and
with no answer for ten seconds it lets no world through. A world's messages come over the same
WebSocket, as a page has no UDP. A page that's silent for a minute, or reads too slowly, is
dropped. The gateway's log has a line for each page as it opens, with where to, and as it closes,
with why and after how long, and one for each page it turns down.

The page takes the gateway from its address: with the stack up (`docker compose up -d`), open

```
http://localhost:8000/nanommo_client.html?gateway=ws://localhost:8090
```

and log in as on a computer. A page from `http://localhost` may open `ws://localhost`. A page
served over HTTPS, as a website is, may only open `wss://`, with a certificate the browser trusts,
which our own authority's isn't (decision 14): in a deployment, a reverse proxy with a public
certificate, such as Caddy's or Let's Encrypt's, takes `wss://` on 443 and passes it to the gateway.

## What's different in a browser

| | On a computer | In a browser |
|---|---|---|
| The servers | `Servers` and `Connection`, over TLS, TCP and UDP | Through the gateway, over a WebSocket, with `WebSocketLink`, the world on the same stream; without one, offline, with `LocalConnection` |
| The page | A window | The game's own, `client/web/shell.html`, in place of Emscripten's: the canvas fills the window, and the page can't scroll, so Space and the arrows reach the game |
| Drawing | OpenGL 3.3, and GLSL 3.30 | WebGL 2, and GLSL ES 3.00: `LoadGlsl` gives each shader its version line and precisions |
| The frame | Our loop, with raylib waiting for the next frame in `WindowShouldClose` | Asyncify turns that wait into one that hands back to the browser, which draws the page |
| Files | Read from beside the program | Preloaded into the page's own file system, at the same paths: the content, the fonts, the translations and the models |
| A texture read back | In its own format | As RGBA bytes, the only way WebGL 2 reads pixels (`FBO_READ_TEXTURE_AS_RGBA`) |
| The bones' matrices | Transposed by OpenGL, with its flag | Transposed on the CPU, by our patch to raylib, as raylib 6.2 does for one matrix there but not for an array |
| The music | The licensed track in `assets/music`, when it's there | Left out, as its licence doesn't let us hand out the file |
| The weather's recordings | The rain, the fire, the gust and the teleport in `assets/sounds`, when they're there | Left out, for the same reason: the weather's sounds are made from numbers, as they are anywhere without the files |

The protocol's login and TLS code, the servers, the bots and the tools aren't in the web build.

## When something goes wrong

| You see | Why | Do |
|---|---|---|
| `The web build runs protoc from NANOMMO_HOST_PROTOC` | There's no release build yet | Build the release preset first |
| A black page, and `Please compile your program with async support` | A build without `-sASYNCIFY` | Build with the preset, whose link options have it |
| `invalid version directive` for a shader | A shader loaded without `LoadGlsl` | Load it through `LoadGlsl` |
| The page doesn't load the client's files | It was opened as a file | Serve the folder, as above |
| `couldn't reach the gateway` on the login screen | The gateway isn't up, or the address is wrong | `docker compose up -d gateway`, and check `?gateway=` |
| The gateway's log says it turned a page down | A path or a world it doesn't know: the world's host and port have to be the ones Consul lists | Check the world list's address |
| Tall spindles stand by each person | raylib without `patches/raylib-webgl-bone-matrices.patch` | Delete `build-web/_deps/raylib-*` and configure again, which fetches and patches raylib |
