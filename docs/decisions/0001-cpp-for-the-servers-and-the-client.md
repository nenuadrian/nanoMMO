# 1. C++20 for the servers and the client

## Status

Superseded by [0026](0026-cpp23-for-the-servers-and-the-client.md)

## Context

Episode 1 set out the architecture, with a native client and a game server, both written in C++,
in a series about building an MMO from scratch in C++. No episode weighs another language, so this
record says what came with the choice rather than why it won.

## Decision

- The game's programs are C++20, built with CMake and Ninja. They are the world server, the login
  server, the saver, the recorder, the bots, the client and the cinema tool. The standard is
  required and the compilers' extensions are off, so the code stays portable, and
  `-Wall -Wextra -Wpedantic` have been on since the first commit. The website (Laravel, episode
  12) and the storyteller (Python, episode 25) sit beside them, in their own languages.
- The code follows Google's C++ style guide, from the first line (see
  [Code style and checks](../checks.md)). The guide doesn't use exceptions, so a function that can
  fail returns `absl::Status` or `absl::StatusOr`, as `TickInterval` did first, in episode 1.
- What two sides need is a library both link, built once, as with the protocol, the content and its
  checks, the terrain, the bag's rules and, from episode 68, how a player walks.

## Alternatives considered

- A language that's memory-safe by default, such as Rust, or one with a garbage collector, such as
  Go or C#, for the servers. These are the obvious alternatives, and none was weighed here.
- For the client, an engine such as Unity, Unreal or Godot, which episode 2 weighed (see
  [0007](0007-draw-the-client-with-raylib-and-imgui.md)).

## Consequences

- The client runs the server's own code where it has to agree with it. That's the bag's rules, to
  show a move before the server answers (0.64.0), and walking, for prediction (0.68.0, see
  [0022](0022-numbered-inputs-and-client-side-prediction.md)). Floats have to round alike on a Mac
  and on Linux for that, so `nanommo_terrain` and `nanommo_walking` build with
  `-ffp-contract=off`, which keeps the compiler from fusing a multiply and an add.
- Memory and threading bugs are ours to find. Episode 28 added AddressSanitizer,
  UndefinedBehaviorSanitizer, ThreadSanitizer, two fuzzers and clang-tidy. UBSan then caught the
  server's test destroying its `io_context` before the server (episode 29), and ThreadSanitizer a
  race in civetweb, which `patches/` fixes.
- Libraries that throw need care, so nlohmann's JSON is read through a helper that checks before it
  asks (episode 21), and cpp-httplib builds with `HTTPLIB_NO_EXCEPTIONS`.
- CMake fetches and builds most libraries from source, pinned (see
  [Versions and dependencies](../releases.md)), so a first build is slow. Three come from the
  system's packages instead. They are libsodium, which doesn't build with CMake, MySQL's client
  library, which comes inside MySQL's whole source tree, and OpenSSL, which Linux distributions
  package.
- C++20's designated initializers leave the fields they don't name at their defaults, which is how
  we use them, so GCC's warning for each of those is off (0.65.0).
