# Changes

What changed in each version of nanoMMO, the newest first.

A version is MAJOR.MINOR.PATCH, from `project()` in `CMakeLists.txt`. While the major version is
0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes one without
changing what the client and the servers say to each other. The login server lets in a client of
its own major and minor version, and turns any other away.

## 0.73.0 (2026-11-15)

- `CMakePresets.json` names each way the game is built: `cmake --list-presets=all` lists them, and
  `cmake --workflow --preset asan` builds the game with the sanitizers and runs the tests. The
  Dockerfile and CI build with the presets, and the sanitized servers in Compose take `PRESET=tsan`
  where they took `SANITIZE=thread`.
- The game is C++23. The servers' timers hand their own lambda to the timer again, without
  `std::function`, and the terrain's and the client's grids are read by row and column,
  `grid[j, i]`, with each index checked against its row or column.
- The sanitizers, the fuzzers, clang-tidy and coverage build with Clang 21, Ubuntu 26.04's own, as
  Clang 18 crashes on the timers' lambdas.
- `./doctor.sh` checks the compiler can do the C++23 the game uses. On Ubuntu 24.04, that's GCC 14.

## 0.72.0 (2026-11-15)

- `nanommo_cheat`, the server's red-team client, tries what a modified client can: more inputs
  than ticks, a direction a thousand times too long or not a number, a swing from 20 metres, a
  skill sent ten times a tick, and a swing cut short to start the next sooner.
- A skill keeps the timings of the weapon it started with, so a swing whose blow has landed plays
  out when its weapon is put in the bag and back, and the next one doesn't start sooner. It did,
  about twice as often.

## 0.71.0 (2026-11-15)

- `docs/decisions` records why the game is built the way it is: a record for each choice that would
  be costly to undo, with what else we weighed and what it costs us.
- The images build from Ubuntu 26.04, and Compose runs MySQL 26.10 and the newest of its other
  services. The C++ libraries, the website's packages and the storyteller's are up to date too.
- A setting's path is moved rather than copied as it's returned, as clang-tidy asks.

## 0.70.0 (2026-11-14)

- Each world leaves out the players and monsters that haven't changed since the newest world the
  client has whole, and names them by their ids: 7.6 KB a second to each of forty bots, from 16.8.
- The bots send their keys each tick, as the client does.

## 0.69.0 (2026-11-14)

- Everyone else is drawn 100 milliseconds in the past, between two worlds, so they move smoothly when
  worlds come late, and carry on for a moment when one is lost.

## 0.68.0 (2026-11-14)

- Your keys walk you at once, and the server's answer corrects you: the client sends its keys each
  tick, numbered, and the server says which it applied.
- The round trip to the server shows under the minimap.
- How a player walks is a library, `nanommo_walking`, which the server and the client both link.

## 0.67.0 (2026-11-14)

- The guild window lists the guild's players, by rank, with their level and who's playing now.
- The leader makes members officers and officers members, removes anyone, and disbands the guild.
  Officers remove members.

## 0.66.0 (2026-11-14)

- Guilds: found one from the guild window, on G, ask players in by name, talk to it with `/g`, and
  leave it, the leader's place going to who's been in longest. Its name shows under each player's.
- Guilds are in MySQL, `V11__guilds.sql`, and invitations in Redis, for a minute.
- A MySQL statement waits two seconds at most for another's lock, and fails as `Aborted`.

## 0.65.0 (2026-11-13)

- `doctor.sh` checks the tools, the libraries, Docker, the ports and the certificates, and ends with
  one command that installs every missing package, with Homebrew or apt.
- The servers, the bots and the client find `tls/` and `content/` in the repository they were built
  from, so they run from any folder.
- A failed handshake says why: a certificate that doesn't name the host asked for, one from another
  authority, or one that's expired, with what to do. A world registered at `localhost`, reached from
  another machine, says to set `NANOMMO_PUBLIC_HOST`.
- `tls/certificates.sh` adds names it's given to the servers' certificate, from the same authority,
  and refuses an address where the folder goes.
- A second server on a machine says its metrics port is taken, where it crashed, and the worlds
  screen says to start one when none is up.
- Compose's MySQL, Redis, ClickHouse, Consul, storyteller and monitoring listen on 127.0.0.1 only.
- GCC builds without a warning for each field a designated initializer leaves out.
- [Getting started](docs/getting-started.md): from a clone to playing, a step at a time.

## 0.64.0 (2026-11-03)

- The hotbar's skills are in slots of brass, each icon in its skill's colour, and dragging one onto
  another swaps them. The client keeps the order in its settings.
- The chat closes into a button, which counts the lines that have come since.
- Items drag for real: the item follows the mouse, each slot lights green where the server would
  take it and red where it wouldn't, and one let go of over no window is thrown away once you say
  so. The client links the server's bag rules for that, and shows a move as soon as it's made,
  until the server's bag comes back.
- The character window lays out what you wear round you, on a lit stage.
- The login, the worlds and the characters are drawn over embers rising in the dark, under the
  game's name, with the character picked on a stage in the middle.
- Loading and dying are in the interface's look.

## 0.63.0 (2026-11-03)

- The interface has a look of its own, in the game's colours and two fonts, Cinzel and Alegreya
  Sans, at a scale that follows the window's height, which can be dragged to any size, and a size
  the player sets, from 75 to 150 percent.
- Every window is a panel with its corners cut and its title in gold, and buttons are drawn to
  match.
- The menu bar is gone. Escape closes what's open, then lets go of the monster picked, then opens
  the game's menu, whose settings have a page for the interface, sound, graphics, the sky and the
  keys. Buttons in the bottom right open the character, the bag, the map and the menu.
- Your frame shows your face, and the monster you picked has its frame at the top in the middle.
- A compass along the top, with the monster you picked, the quest givers, the gates and where
  you're walking to on it.
- The minimap is round and in the top right, zooms, and can turn with the camera. The quests are
  under it.

## 0.62.0 (2026-11-03)

- A skill's strides carry the player along the way they faced as it started, on the server, so that
  everyone sees the dash.
- A blow can throw a monster up, keep it in the air, or dash it down. A monster's in the air for two
  seconds at most, and then stands its ground for two more. The world message says how high each
  monster is, so a client of 0.61 can't read it, and the login server turns it away.
- Three more kinds of fx: copies left behind a fast move, a flash over the view of the player using
  a skill, and their camera going in.
- Five sword skills, from the trainer: Phantom Rush, Skyrend, Tempest Flurry, Cyclone and Severing
  Moon.

## 0.61.0 (2026-11-03)

- A skill can land more than once, each blow at its own time and with its own push, sharing the
  skill's damage out between them, and Whirlwind lands one each time round. Every blow knocks what it
  lands on back a little, over 0.2 seconds, where it used to jump there in one tick.
- A blow that lands holds the player and what it hit still for a moment, from 50 to 90 milliseconds
  and longer for one that pushes further, and they catch up after it. The monster lights up, and
  rocks back away from whoever hit it.
- Two more kinds of fx: the blade lit along its edge, and cuts that hang in the air, each with a sound
  of its own. Thrust, Whirlwind and Earthsplitter light their blades, and Thrust and Earthsplitter
  cut the air.
- Space goes round a chain of three swings, Strike, Return Cut and Overhead Cut, each taking a share
  of the weapon's swing and of its damage, so that holding Space hurts as much over a second as
  before.

## 0.60.0 (2026-11-03)

- The people are built on a rig of our own, in `content/rig.txtpb`: a skeleton of 24 joints, with an
  adult's proportions, in two builds, the second broader at the shoulders, and how a person stands,
  idles, walks, runs and dies. The client works out the motions as it loads, so there's nothing to
  download, and a fresh clone draws everyone as a person, never as a cube.
- The walk and the run come from where the feet go: a foot that's down stays where it was put down,
  as the server moves the body on, and the legs bend at the knee to reach it, so the feet don't
  slide.
- In the death, the person falls back, sits down hard and lies on the ground, never under it.
- The content's checks refuse a part of a figure, a way to wear the hair or an item on a joint the
  rig doesn't have, and the rig is in the clients' content hash.
- Mixamo's files, the Dockerfile's `characters` stage and `assets/tools`, which converted them, are
  gone.

## 0.59.0 (2026-11-03)

- The game has a version, one for the client and the servers. The login screen shows it in its
  corner, the client and the servers print it as they start, and the world servers' metrics carry
  it, as the label of `nanommo_build_info`.
- The login server turns away a client of another major or minor version, and says which version
  to get. A client from before versions is told it's too old.
- `dependencies.py` lists the C++ libraries `CMakeLists.txt` fetches, each with the version it's
  pinned to and the newest, and with `--update`, moves them on, each pinned by the SHA-256 of what it
  downloads.
- abseil 20260817.0, raylib 6.2, Dear ImGui 1.93.0, hiredis 1.5.0 and cpp-httplib 0.64.0. raylib
  6.2's `UnloadModel` frees a model's pose and bone matrices too, so the client stops freeing them
  after it.
- Once a week, Dependabot opens pull requests for the actions, the images, the website's PHP
  packages and the storyteller's Python ones, and a workflow opens one for the C++ libraries.
