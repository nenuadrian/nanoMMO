# 26. C++23 for the servers and the client, with each build named in a preset

## Status

Accepted

## Context

[0001](0001-cpp-for-the-servers-and-the-client.md) made the game's programs C++20. By episode 73,
two parts of C++23 would tidy code we had. The heartbeat, the wait for players to leave and the
saver's tick each hand their lambda to their timer again, and as a lambda couldn't name itself, each
was a `std::function`. Five grids kept in one array, the terrain's corners and trees, the map's
pixels, the chunks and the heights the sea reads, each worked out `j * size + i` by hand.

The game was also built seven ways, release, two sanitizers, the fuzzers, clang-tidy, coverage and
profiling, each with its flags spelled out in a stage of the Dockerfile and again in the docs, all
into one `build/` folder.

## Decision

- The game's programs are C++23 (`CMAKE_CXX_STANDARD 23`), required, without the compilers'
  extensions. The rest of 0001 holds: CMake and Ninja, Google's style guide, `absl::Status` for what
  can fail, and a library for what two sides share.
- We use a part of C++23 once the compilers we build with have it, in the versions our platforms
  ship: GCC 15 and Clang 21, Ubuntu 26.04's, in Docker and CI, Xcode on macOS, and GCC 14 on Ubuntu
  24.04. Of the parts that came late, the game uses a member function that takes its object as a
  parameter, `this auto self`, and a subscript operator with two indexes, `grid[j, i]`.
  `doctor.sh` asks the compiler to compile both.
- `Grid` (`terrain/terrain.h`) is our own view of a grid kept in one array, and checks each index
  against its row or column with abseil's `CHECK`, in each build.
- `CMakePresets.json` names each way the game is built. `release` builds into `build/`, as before,
  and the others each into a `build-<name>/` folder. The Dockerfile, CI and the docs build with
  them.

## Alternatives considered

- `std::mdspan` for the grids. libc++ has had it since LLVM 17, and it built on the Mac, but GCC's
  library gets it in GCC 16, and Ubuntu 26.04 ships GCC 15 (episode 73, step 7). It would also read
  a column past the end of its row from the next row, unless the standard library's own checks were
  on, where `Grid` checks each index in each build.
- `std::expected` in place of `absl::StatusOr` (the obvious one, not weighed in an episode).
  StatusOr carries a code and a message already, which each `std::expected` would have to spell out.
- Staying on C++20, with the `std::function`s and the arithmetic as they were.

## Consequences

- Ubuntu 24.04 builds with `g++-14`, as its GCC 13 can't compile `this auto self`, and Ubuntu 22.04's
  compilers can't build the game ([Getting started](../getting-started.md)).
- A lambda that takes its own object, with its return type deduced, can't be handed to Asio's
  `async_wait` before that type is known: async_wait checks it, and deducing it needs the body, which
  calls async_wait (episode 73, step 5). Those lambdas name it, `-> void`.
- Clang 18 crashes generating the code for those lambdas, where each hands its own object to
  async_wait (episode 73, step 6), so the Dockerfile's sanitizers, fuzzers, clang-tidy and coverage
  build with Clang 21, Ubuntu 26.04's own, and building the checks takes Clang 19 or newer.
- `Grid` compares two indexes on each read, and a bad one stops the program where it would have read
  a cell of the next row.
- A preset's flags are in one place, and a local sanitizer build doesn't rebuild the release one.
  `build-*/` is ignored by git and by Docker.
