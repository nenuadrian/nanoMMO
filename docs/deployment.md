# Deployment

The game runs on any Linux server with Docker and Docker Compose, such as a VPS, from
`compose.prod.yaml` on top of `compose.yaml`. On the server, `.env` sets
`COMPOSE_FILE=compose.yaml:compose.prod.yaml`, so each `docker compose` command there reads both.

- The internet reaches the login server on TCP port 7778, the worlds on ports 7777 and 7787, TCP and
  UDP, and their next instances on 7877 and 7887, and the website on port 8080, and nothing else. MySQL, Redis, the storyteller and the worlds'
  metrics stay on Compose's own network. Docker publishes a port past a firewall such as ufw, so leaving a port out of
  the file is what keeps it off the internet.
- Prometheus, Grafana, Loki, Alertmanager, Mailpit and Consul listen on the server's localhost. Reach
  them through SSH: `ssh -L 3000:localhost:3000 -L 9090:localhost:9090 -L 9093:localhost:9093 -L
  8025:localhost:8025 -L 8500:localhost:8500 debian@203.0.113.7`, then open
  http://localhost:3000, where Grafana reaches Loki on Compose's network.
- MySQL's password and the website's `APP_KEY` come from `.env` on the server, which `deploy.sh` makes
  on the first deploy, with random ones, readable by the deploying user alone. Git and the Docker
  builds leave `.env` out.
- The worlds' public host comes from `.env` too, as `NANOMMO_PUBLIC_HOST`: `deploy.sh` adds it, from
  the address it deploys to, when `.env` doesn't have it yet.
- The services start again after a crash or a reboot (`restart: unless-stopped`).

The certificates come from an authority of your own, made on your machine, with the server's address
on them. `deploy.sh` sends the servers their certificate and key, and the authority's key stays with
you. The clients and the bots trust the authority's certificate:

```bash
tls/certificates.sh tls/vps 203.0.113.7                 # once
./deploy.sh debian@203.0.113.7 tls/vps                  # this commit, or name one: ... tls/vps ep019/end
NANOMMO_TLS_CA=tls/vps/ca.crt ./build/bin/nanommo_client 203.0.113.7
```

`deploy.sh` sends the commit with `git archive`, so the server needs no git or access to the repo,
with the certificates added under `tls/`, all in one SSH connection, so that a deploy right after
other SSH commands isn't refused: ufw's `limit` on SSH refuses a sixth connection within 30 seconds.
The server unpacks it into `nanommo.next`, and `release.sh` there starts it: it keeps `.env`, and
builds the images first. The tests run as part of the build, so a release that fails them, or
doesn't build, leaves the running release as it is. Then the release before moves into
`nanommo.previous`, the new one into `nanommo`, and `docker compose up -d --wait --force-recreate`
starts each container again from the new release's folder: one left running would keep the folder it
started from.

`release.sh` starts any release unpacked into `nanommo.next` next to `nanommo`, so something on
the server can deploy too. A release without certificates keeps the running release's. The first
release on a server comes from `deploy.sh`, which passes the server's address, and `release.sh`
makes `.env` for it. The worlds start again
through `handover.sh` (see [Deploying without downtime](#deploying-without-downtime)), so their
players play on through a deploy. The images build on the
server from the source, protobuf and all: about fifteen minutes on 2 vCPUs the first time. After
that, the server's Docker keeps ccache's cache mount (see [Checking the code](checks.md#checking-the-code)),
so a deploy compiles what changed. To go back to the release before, which builds its images again:

```bash
ssh debian@203.0.113.7 'rm -rf nanommo && mv nanommo.previous nanommo && cd nanommo && docker compose up -d --build --wait --force-recreate'
```

The volumes, with the accounts and the players, belong to the Compose project, not to a release, so
they stay through each deploy, and Flyway runs any new migrations before the servers start. Going
back leaves the database as the newer release left it, as Flyway skips migrations it doesn't know,
so a migration has to keep the release before it working. New
certificates need the servers to start again: `ssh debian@203.0.113.7 'cd nanommo && docker compose
restart login server frost'`.

The benchmark runs on the server too, with the bots there, as `deploy.sh` sends the authority's
certificate along for them. It needs `jq` on the server (`sudo apt install jq`), and `curl`, which
Debian has:

```bash
ssh debian@203.0.113.7 'cd nanommo && bench/load.sh 100 10' > bench/results/vps.json
```

On a VPS with 2 vCPUs, which the bots share with the servers, the game server keeps its 20 ticks a
second up to about 700 players (see
[What each player gets](networking.md#what-each-player-gets)).

## Deploying without downtime

Restarting a world throws its players out to the login screen, and a deploy restarts the worlds.
Instead, a world hands its players over to its next instance:

- The next instance starts next to it, on a port of its own, and registers with Consul, where it's
  the world's newest instance, so the login server lists it.
- On `SIGTERM`, a world leaves Consul, and asks it for the world's newest instance, which is now
  the next one. If there is one, and the world has players, it hands them over: it stops ticking
  and taking new players, saves each player into Redis, where the next instance loads them from,
  and sends each client a `Move`, with the host and port players reach the next instance at, and a
  token to say hello there with. Tokens are one-time, and the one each player said hello with is
  used, so the world issues one for each move, in Redis like the login server's. Then it waits up to ten seconds for its clients to leave, and
  stops.
- Without a next instance, a world stops as before.
- The client connects to the next instance as soon as it gets the `Move`, and keeps drawing the
  world it has until the next instance sends it one, so the world stays on the screen. Then it goes
  over, and closes its connection to the instance before, which lets that one stop.

In Compose, each world has a next instance, `server-next` on port 7877 and `frost-next` on 7887,
in the `next` profile, so `docker compose up` leaves them out. On Compose's network, each next
instance answers to its world's name too, so `server` has two addresses while both run. Each world's
health check asks Consul for its instance, so a world is healthy once Consul lists it, and
`docker compose up --wait` waits for that. `handover.sh` starts a world again from its newest image
while its players play on: it starts the next instance and waits until it's healthy, stops the
world, which hands its players over, starts the world again and waits for it, and stops the next
instance, which hands them back. A world hands its players over to its newest instance in Consul, so
without the wait, a world could stop before the instance it hands over to has registered, and its
players would end up at the login screen. So the world ends up where it was, on its own port, and
the next instance is stopped, as it should be: two instances of a world that both took players would
split them in two. Alloy gives the next instances' lines a level like the worlds', and the
dashboard's log panels have them too.

```bash
docker compose --profile next build
./handover.sh server
```

The bots follow a move too, like a client: a bot whose world hands it over closes its connection,
and a new one with its number says hello to the next instance with the move's token. The bots play
against a single machine, so a bot keeps the host it was given and takes the move's port. In
Compose, `server` is both instances' name while both run, and asio's connect tries each address a
name has, so a bot reaches the one on the move's port. Each swarm serves how many of its
bots are connected, as `nanommo_bots_connected`, where a handover shows as a dip of a second at
most, and the dashboard adds a world's players up across its instances.

`deploy.sh` starts everything else again as before, and each world through `handover.sh`. Compose
gives a world 15 seconds to stop before it kills it, which leaves the ten seconds the handover waits
for. On a server, Compose publishes the next instances' ports like the worlds', TCP and UDP, so they're
open to the internet too.

## Deploying on push

A runner on the server deploys each push to `main`, once CI passes. It's GitHub Actions' own
runner, self-hosted: it connects to GitHub and waits there for jobs with its labels, `self-hosted`
and `nanommo`, so the server opens no port for it. Its image, in `runner/`, is GitHub's runner
image with Docker Compose added, and it reaches the server's Docker through Docker's socket, which
belongs to the server's `docker` group: `runner/start.sh` passes the group's number to the
container, which has no such group of its own.

The runner is in a Compose project of its own, `nanommo-runner`, so a deploy, which starts each
container of the game's project again, leaves alone the runner it runs on. The first time it starts,
it registers with the repository, with a registration token that lasts an hour, and its container
keeps the registration from then on, through restarts and reboots. To start it on a server the game
runs on:

```bash
token=$(gh api -X POST repos/nenuadrian/nanoMMO/actions/runners/registration-token --jq .token)
ssh debian@203.0.113.7 "nanommo/runner/start.sh https://github.com/nenuadrian/nanoMMO $token"
```

The repository's settings list it under Actions, Runners. A change to its image or to
`runner/compose.yaml` makes a new container, which registers again, with a new token: the same two
commands. CI builds its image too.

The `deploy` job in `.github/workflows/ci.yml` runs on it, for a push to `main`, once the build,
clang-tidy, the sanitizers and the fuzzers pass. A job waits for a runner with its labels, for a day, so it runs
only in a repository whose `NANOMMO_DEPLOY` variable is `true`, which leaves it out of forks
without a runner:

```bash
gh variable set NANOMMO_DEPLOY --body true --repo nenuadrian/nanoMMO
```

The job unpacks the commit into `nanommo.next`, as `deploy.sh` does, and starts it with
`release.sh`, which keeps the running release's `.env` and certificates. The releases are in the
home folder of the user who deploys, which isn't the runner's: its container has a home of its own.
So the runner mounts the home folder of the user who started it, and the job finds it in
`NANOMMO_HOME`. It's mounted at the same path as on the server, because Compose hands the server's
Docker the paths of what a container mounts, such as Flyway's `./db`, from the folder Compose runs
in, and Docker looks for them on the server. The runner works as that user, so that it can write
there, and what it writes is theirs, which `deploy.sh` can replace. The container may have no home
folder for that user, so the runner's folder is their home there, where Docker and git keep their
settings. Deploys go one at a time:
a push while one runs waits for it, and a newer push takes the place of one that's waiting, so the
newest commit is deployed, and no deploy stops halfway.

The runner can do anything on the server that Docker can, which is anything root can, so it runs our
deploy and nothing else. The repository is public, and a pull request from a fork runs the workflow
as the pull request has it, so it could send a job of its own to the runner, by its labels, which is
why GitHub recommends self-hosted runners for private repositories. So before each job, the runner
runs `runner/job-started.sh`, from its image, which no pull request can change: it refuses any job
that isn't for a push to `main`, and a job it refuses fails before its first step. Only someone who
can push to `main` can give the runner a job, and they can change the game anyway. The
repository's settings, under Actions, General, also have workflows from forks wait for our approval
("Require approval for all external contributors"), so that we read a pull request before anything
in it runs.

```bash
GITHUB_EVENT_NAME=pull_request GITHUB_REF=refs/pull/7/merge runner/job-started.sh   # refused
GITHUB_EVENT_NAME=push GITHUB_REF=refs/heads/main runner/job-started.sh             # taken
```
