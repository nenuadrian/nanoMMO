# 4. Build, test and check in Docker, on every push

## Status

Accepted

## Context

In episode 1, the server built on a Mac, with Apple's compiler. It was going to run on Linux, so
that's where it should be built and tested, the same way on any machine, CI's included.

## Decision

- One multi-stage `Dockerfile`, from Ubuntu's long-term support release, which the servers run
  on too. It has a toolchain, a build, and the tests. Each server's image copies its program from
  the test stage, so an image builds only when every test passes.
- Each check is a stage, which `docker build --target <stage> .` runs. They are `lint`,
  clang-format and cpplint, pinned, in a slim Python image, and `docs` (episode 1), then
  `sanitize`, `fuzz`, `tidy` and `coverage` (episode 28).
- GitHub Actions builds the same stages a developer would, on each push and pull request
  (`.github/workflows/ci.yml`).
- Docker Compose runs the services side by side, from episode 4. They are the servers, and what
  they need, MySQL, Redis, Consul and the monitoring. Tests that need MySQL and Redis run next to
  them, as `docker compose run --rm tests` (episode 5).
- Each dependency is pinned, the libraries to a release, a tag or a commit, with the SHA-256 of
  what CMake downloads since episode 59, the images to their tags, the actions and the tools to
  their versions ([Versions and dependencies](../releases.md)).

## Alternatives considered

- Building and testing on each developer's machine as it is, which is where episode 1 started.
- Installing the toolchain straight onto CI's runners, without Docker. This is the obvious
  alternative, and it wasn't weighed. CI and a laptop would then build two different ways.

## Consequences

- The client's tests open a window, so the test stage runs them on Xvfb, a virtual display, and
  llvmpipe, Mesa's OpenGL on the CPU, without a screen or a GPU (episode 2). ThreadSanitizer then
  needs `tsan.supp` for Mesa's threads.
- Docker Desktop's kernel isn't Ubuntu's, so the profile stage puts the generic perf on the path
  (episode 8), and ThreadSanitizer needs `vm.mmap_rnd_bits` lowered first
  ([Code style and checks](../checks.md)).
- Each build compiled protobuf and the rest again, until episode 43 put ccache in a cache mount
  that outlives the build, which CI moves in and out of GitHub's cache.
- A private repository pays for CI's minutes, so from episode 43 its pushes get the tests, and the
  slow checks run at night. Either way, a check that passed on the same tree doesn't run again.
- The images build on the server for a deploy, and run the tests there first
  ([0016](0016-deploy-with-compose-to-one-server.md)).
- Native builds still work, on macOS and Ubuntu ([Getting started](../getting-started.md)), with
  what the images install. On a Mac, CMake asks Homebrew where `mysql-client` and OpenSSL are.
- `compose.yaml` and `monitoring/` stay out of the build's context, so changing them doesn't
  rebuild the servers (episode 4).
