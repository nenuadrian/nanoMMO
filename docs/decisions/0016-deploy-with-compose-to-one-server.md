# 16. Deploy with Compose to one server, and hand the players over

## Status

Accepted

## Context

Episode 19 took the game onto a server of its own, a VPS with 2 vCPUs. Each deploy then restarted
the worlds, and threw everyone playing out to the login screen, so episode 36 deployed without
that, and episode 37 deployed each push to `main`.

## Decision

- The game runs on any Linux server with Docker and Compose, from `compose.prod.yaml` on top of
  `compose.yaml`, with its passwords in a `.env` that the first deploy makes, at random.
- `deploy.sh` sends a commit with `git archive`, with the servers' certificates, over one SSH
  connection. The server builds the images from the source, tests and all, before anything
  restarts, and keeps the release before next to the new one, to go back to.
- A world restarts through its next instance (`handover.sh`). The next instance starts on a port of
  its own and registers with Consul. On `SIGTERM`, the world saves its players into Redis, and
  sends each client a `Move`, with the next instance's address and a fresh token, and the client
  goes over without losing the world on its screen.
- A GitHub Actions runner on the server, in a Compose project of its own, deploys each push to
  `main` once CI passes, and refuses any other job (`runner/job-started.sh`).

## Alternatives considered

- A platform that runs containers for us. They're made for web services, and a lot of them don't
  take UDP, or connections that stay open for hours (episode 19).
- Kubernetes, which runs containers across a cluster, and is a lot to look after for one server
  (episode 19).
- CI building the images once, and pushing them to a registry for each server to pull. That's the
  way with several servers. With one, building where it runs keeps it to SSH and Docker
  (episode 19).
- Restarting the worlds and letting the players log in again, as in episodes 19 to 35.

## Consequences

- Each deploy builds the C++ on the cores the game runs on: about fifteen minutes the first time,
  protobuf and all, and after that, ccache's mount keeps it to what changed.
- A build or a test that fails leaves the running release as it is.
- The database is the Compose project's, not a release's, so a migration has to keep the release
  before it working ([0015](0015-migrations-with-flyway.md)).
- A world runs as two instances for a moment, so each registers as its name and port, such as
  `Ember-7777`, and the login server lists the newest
  ([0018](0018-find-the-worlds-through-consul.md)). The next instances' ports are open to the
  internet too, for TCP and UDP.
- Tokens work once, so a world issues one for each move (episode 36), and Compose gives a world 15
  seconds to stop, room for the ten the handover waits for its clients.
- The runner can do anything on the server that Docker can, which is anything root can, and a pull
  request from a fork runs the workflow as it has it. So the runner refuses every job but a push to
  `main`, and workflows from forks wait for our approval ([Deployment](../deployment.md)).
