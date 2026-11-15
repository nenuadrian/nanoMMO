# 20. One version for the whole game, and the login server's rule

## Status

Accepted

## Context

The top `CMakeLists.txt` had given the project a version since episode 1, 0.1.0, and nothing but
the API docs' header read it. In episode 59, a client of another version would log in, then fail
to make sense of what the world sent it, and a player who reports a bug needs to say which version
they're playing.

## Decision

- The game has one version, for the client and all the servers: `project(nanommo VERSION ...)`.
  CMake writes it into `protocol/version.h` as it configures the build, as `nanommo::kVersion`.
- A version is MAJOR.MINOR.PATCH, after Semantic Versioning. While the major version is 0, each
  episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes a minor version
  without changing what the client and the servers say to each other.
- The client sends its version with each login, and the login server lets it in when its major
  and minor match the servers' (`Compatible`). It turns any other away with the version to get,
  and tells a client from before versions, which sends none, that it's too old.
- The version shows in the corner of the login screen, the client, the login server and the world
  server print it first thing, and it labels each world's `nanommo_build_info`.
- A release sets the version, adds its section to `CHANGELOG.md`, and tags its commit, such as
  `v0.59.0` ([Versions and dependencies](../releases.md)).

## Alternatives considered

- Letting any client in, as protobuf's older code skips the fields it doesn't know. A client of
  another version would log in, and then fail to make sense of the world (episode 59).
- A protocol version kept apart from the game's. This is the obvious alternative, and it wasn't
  weighed.

## Consequences

- A deploy that changes the minor version turns away the clients built before it, at their next
  login. 0.62.0's worlds say how high each monster is, which a client of 0.61 can't read, so the
  login server turns that client away.
- A version CMake takes, such as 0.60 without its patch, would turn away every client, our own too,
  so a test checks that our own version is compatible with itself.
- The bots log in through the login server, so they send the version too, or a deploy would turn
  away the load test.
- While a deploy hands the worlds over, `nanommo_build_info` shows which instances run the new
  version ([0016](0016-deploy-with-compose-to-one-server.md)).
- Each episode that changes code moves the minor version, in a step of its own or with its first
  change, and adds what changed to the changelog before its end.
