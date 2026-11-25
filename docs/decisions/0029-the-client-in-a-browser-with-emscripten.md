# 29. The client in a browser, with Emscripten and WebGL 2

## Status

Accepted

## Context

Playing the game took a build on a computer, and the servers. A page anyone can open asks for
neither, and the client is C++ on raylib, which runs in a browser through WebAssembly.

## Decision

- The client builds to WebAssembly with Emscripten (`emcmake cmake --preset web`), the same
  sources as on a computer, and draws with WebGL 2, through raylib's web platform.
- What a browser can't do goes behind a seam, not through the code, with the network behind
  `Servers` and `Link` (in a browser, there's no way to the servers yet), and the shaders' GLSL
  behind `LoadGlsl`. The few settings that differ are in the web build's CMake. They are Asyncify,
  the preloaded files, and reading textures back as RGBA.
- The web build runs the release build's `protoc`, of the same version, as one built as
  WebAssembly can't run while building.

## Alternatives considered

- A client written for the web, in TypeScript with a WebGL library (the obvious one, not weighed in
  an episode). It would be a second client to keep in step with the first, and with the server.
- WebGPU, which browsers have been adding. raylib draws with OpenGL, and WebGL 2 is in each browser
  we'd want.

## Consequences

- The web build needs the release build first, for `protoc`.
- Each shader keeps to what GLSL 3.30 and GLSL ES 3.00 share, as the same source compiles as both.
- Asyncify makes the WebAssembly bigger and slower in the functions it rewrites, for a loop that
  stays as it is.
- The licensed music doesn't go into the page.
