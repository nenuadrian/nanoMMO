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

A page plays offline from the start, as `CanReachServers()` is false in a browser. On a computer,
`./build/bin/nanommo_client --offline` does the same, without the servers or a certificate.

| Offline, there's | Not there, as it lives on the servers |
|---|---|
| The Meadows, with its monsters and NPCs, and one player, Traveller | Other players, and the other lands: a gate says so in the chat |
| Fighting, skills, levels, the bag and its gold, and the NPCs' skills and quests | The storyteller's stories, guilds, and trades, which take another player |
| Everything the client draws and plays | Saving: a page left or closed starts again |

## What's different in a browser

| | On a computer | In a browser |
|---|---|---|
| The servers | `Servers` and `Connection`, over TLS, TCP and UDP | None: a page can't open a socket, so it plays offline, with `LocalConnection` |
| Drawing | OpenGL 3.3, and GLSL 3.30 | WebGL 2, and GLSL ES 3.00: `LoadGlsl` gives each shader its version line and precisions |
| The frame | Our loop, with raylib waiting for the next frame in `WindowShouldClose` | Asyncify turns that wait into one that hands back to the browser, which draws the page |
| Files | Read from beside the program | Preloaded into the page's own file system, at the same paths: the content, the fonts and the models |
| A texture read back | In its own format | As RGBA bytes, the only way WebGL 2 reads pixels (`FBO_READ_TEXTURE_AS_RGBA`) |
| The bones' matrices | Transposed by OpenGL, with its flag | Transposed on the CPU, by our patch to raylib, as raylib 6.2 does for one matrix there but not for an array |
| The music | The licensed track in `assets/music`, when it's there | Left out, as its licence doesn't let us hand out the file |

The protocol's login and TLS code, the servers, the bots and the tools aren't in the web build.

## When something goes wrong

| You see | Why | Do |
|---|---|---|
| `The web build runs protoc from NANOMMO_HOST_PROTOC` | There's no release build yet | Build the release preset first |
| A black page, and `Please compile your program with async support` | A build without `-sASYNCIFY` | Build with the preset, whose link options have it |
| `invalid version directive` for a shader | A shader loaded without `LoadGlsl` | Load it through `LoadGlsl` |
| The page doesn't load the client's files | It was opened as a file | Serve the folder, as above |
| Tall spindles stand by each person | raylib without `patches/raylib-webgl-bone-matrices.patch` | Delete `build-web/_deps/raylib-*` and configure again, which fetches and patches raylib |
