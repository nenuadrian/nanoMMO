# Versions and dependencies

## Versions

The game has one version, for the client and all the servers: `project(nanommo VERSION ...)` in
the top `CMakeLists.txt`. CMake writes it into `protocol/version.h`, in the build folder, from
`protocol/version.h.in`, as it configures the build, so the code has it as `nanommo::kVersion`.

- The client shows it in the bottom right corner of the login screen.
- The client, the login server and the world server print it first thing as they start:
  `nanommo server: version 0.59.0`.
- Each world server's metrics carry it, as the `version` label of `nanommo_build_info`, which is
  always 1. While a deploy hands the worlds over one at a time, `nanommo_build_info` shows which of
  them run the new version.

A version is MAJOR.MINOR.PATCH, after [Semantic Versioning](https://semver.org/). While the major
version is 0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes a
minor version without changing what the client and the servers say to each other.

The client sends its version with each login (`LoginRequest.version`), and the login server lets
it in when its major and minor version match the servers' (`Compatible`). It turns any
other away, with an error that names the version to get, and a client from before versions, which
sends none, is told it's too old. So a deploy that changes the minor version turns away the
clients built before it, at their next login.

## Releases

A release changes the version in `CMakeLists.txt`, adds it to `CHANGELOG.md`, with its date and
what changed, and tags its commit `v` and the version, such as `v0.59.0`.

## Dependencies

Each dependency is pinned, so that a build is the same from one day to the next: the C++ libraries
to a release, a tag or a commit, each with the SHA-256 of what CMake downloads (`URL_HASH`), the
images to their tags, the actions to their versions, and the website's and the storyteller's
packages in `composer.lock` and `requirements-server.txt`.

`dependencies.py` lists the libraries `CMakeLists.txt` fetches from GitHub, the version each is
pinned to, and the newest: the latest release, or the newest tag that's a version, for a library
without releases, or the newest commit, for one pinned to a commit. With `--update`, it writes the
newest into `CMakeLists.txt`, with its hash. It reads `GITHUB_TOKEN`, when it's set, for GitHub's
higher limit on requests.

```bash
./dependencies.py             # what's behind
./dependencies.py --update    # moves each to its newest, pinned by its hash
```

Once a week, in the public repository:

- Dependabot (`.github/dependabot.yml`) opens a pull request for each kind of dependency it can
  read with a newer version: the actions, the images in the Dockerfiles and the compose files, the
  website's PHP packages and the storyteller's Python ones. The minor and patch updates of a kind
  go in one pull request, and each major update in one of its own.
- The Dependencies workflow (`.github/workflows/dependencies.yml`) runs `dependencies.py --update`
  and opens a pull request, `dependencies/cmake`, with what it changed, or brings the open one up to
  date. A pull request opened with the workflow's token starts no workflows, so it starts CI on
  the branch by hand. It needs the repository's Actions settings to allow GitHub Actions to create
  pull requests.

CI checks each pull request like any other. The updates that pass go into the game in the next
episode, as a commit of their own, and the pull request is closed.
