# Decisions

This folder keeps nanoMMO's decision records: a page for each choice that shaped the game, with
what forced it, what we picked, what else we weighed, and what it cost. They follow Michael
Nygard's architecture decision records, kept short.

## Why keep them

The code says what the game does, and a commit says what changed. Neither says why it's this way
and not another, and that's what gets lost first.

- The why survives the people who knew it. A choice made in episode 13 still shapes the game,
  and its record keeps the reasons and the numbers for whoever comes after the people who weighed
  it.
- Newcomers learn the shape of the code. Read top to bottom, the records are the game's
  architecture in the order it was built, each with the problem it answered.
- Settled questions stay settled. "Why not a JSON web token?" has an answer in
  [0011](0011-a-login-server-of-its-own.md), and asking again means saying what's changed since.
- Reversals are deliberate. Changing a decision takes a new record that says what's different
  now, as [0007](0007-draw-the-client-with-raylib-and-imgui.md) did for
  [0006](0006-draw-the-client-with-vulkan.md), so the game never drifts off one by accident.

## When to write one

Write a record for a choice that would be costly to reverse, such as a language, a database, the
wire format or how the servers find each other, or one that people will ask about. A change that
one commit could undo, or that the code makes plain, needs none.

## How

- One file for each decision, named with the next number and a short title, such as
  `0025-a-short-title.md`, from [template.md](template.md).
- Its sections are the status, the context, with the episode it came up in, the decision, the
  alternatives considered, and the consequences. An alternative goes in only if it was weighed,
  or if it's the obvious one, and then it says it wasn't weighed. Name the files, the settings and
  the numbers, with where each number was measured.
- A record is accepted as it's merged, and never rewritten after, like a published step
  ([0005](0005-the-history-is-the-curriculum.md)) or a migration that has run
  ([0015](0015-migrations-with-flyway.md)). To change a decision, write a new record, and set the
  old one's status to `Superseded by` the new one. That line is the only edit an accepted record
  gets.
- Add it to the index below.

## Index

| Number | Title | Status | Episode |
|---|---|---|---|
| [0001](0001-cpp-for-the-servers-and-the-client.md) | C++20 for the servers and the client | Superseded by [0026](0026-cpp23-for-the-servers-and-the-client.md) | 1 |
| [0002](0002-an-authoritative-server-at-a-fixed-tick.md) | An authoritative server, at a fixed tick | Accepted | 1, 3 |
| [0003](0003-protobuf-as-the-contract.md) | Protobuf as the contract between the client and the servers | Accepted | 1, 3 |
| [0004](0004-build-test-and-check-in-docker.md) | Build, test and check in Docker, on every push | Accepted | 1, 4, 28, 43 |
| [0005](0005-the-history-is-the-curriculum.md) | The repository's history is the curriculum | Accepted | 1 |
| [0006](0006-draw-the-client-with-vulkan.md) | Draw the client with Vulkan | Superseded by [0007](0007-draw-the-client-with-raylib-and-imgui.md) | 2, as first written |
| [0007](0007-draw-the-client-with-raylib-and-imgui.md) | Draw the client with raylib, and its interface with Dear ImGui | Accepted | 2 |
| [0008](0008-one-thread-for-the-world.md) | One thread for the world, on Asio's event loop | Accepted | 3 |
| [0009](0009-prometheus-grafana-and-loki.md) | Metrics, logs and alerts with Prometheus, Grafana and Loki | Accepted | 4, 30, 35 |
| [0010](0010-mysql-as-the-source-of-truth.md) | MySQL as the source of truth | Accepted | 5 |
| [0011](0011-a-login-server-of-its-own.md) | A login server of its own, with Argon2id and one-time tokens | Accepted | 5, 6 |
| [0012](0012-tcp-for-inputs-udp-for-worlds.md) | TCP for inputs and messages, UDP for the worlds | Accepted | 3, 9 |
| [0013](0013-save-players-through-redis.md) | Save the players through Redis, and a saver that takes them to MySQL | Accepted | 13 |
| [0014](0014-tls-with-an-authority-of-our-own.md) | TLS 1.3 between the clients and the servers, with an authority of our own | Accepted | 14 |
| [0015](0015-migrations-with-flyway.md) | Change the tables with Flyway's migrations | Accepted | 18 |
| [0016](0016-deploy-with-compose-to-one-server.md) | Deploy with Compose to one server, and hand the players over | Accepted | 19, 36, 37 |
| [0017](0017-send-each-player-what-is-near-them.md) | Send each player what's near them, by cells | Accepted | 20 |
| [0018](0018-find-the-worlds-through-consul.md) | Find the worlds through Consul | Accepted | 21 |
| [0019](0019-content-as-data.md) | Content as data, in protobuf's text format | Accepted | 26, 27 |
| [0020](0020-one-version-for-the-whole-game.md) | One version for the whole game, and the login server's rule | Accepted | 59 |
| [0021](0021-guilds-in-mysql-transactions.md) | Guild changes as MySQL transactions, and invitations in Redis | Accepted | 66, 67 |
| [0022](0022-numbered-inputs-and-client-side-prediction.md) | Numbered inputs, and client-side prediction | Accepted | 68 |
| [0023](0023-draw-everyone-else-in-the-past.md) | Draw everyone else 100 milliseconds in the past | Accepted | 69 |
| [0024](0024-leave-out-what-has-not-changed.md) | Leave out of each world what hasn't changed, by its pieces' hashes | Accepted | 70 |
| [0025](0025-the-server-checks-what-a-client-asks-for.md) | The server checks what a client asks for, and a red-team client tries it | Accepted | 72 |
| [0026](0026-cpp23-for-the-servers-and-the-client.md) | C++23 for the servers and the client, with each build named in a preset | Accepted | 73 |
| [0027](0027-gold-in-the-bag.md) | Gold in the bag's message, with its revision | Accepted | 74 |
| [0028](0028-a-trade-is-checked-against-the-bags-and-saved-as-one.md) | A trade is checked against the bags, and saved as one | Accepted | 75 |
| [0029](0029-the-client-in-a-browser-with-emscripten.md) | The client in a browser, with Emscripten and WebGL 2 | Accepted | 76 |
| [0030](0030-offline-the-client-runs-the-worlds-rules.md) | Offline, the client runs the world's rules itself | Accepted | 78 |
| [0031](0031-a-character-is-leased-to-one-instance-at-a-time.md) | A character is leased to one instance at a time | Accepted | 79 |
