# Changes

What changed in each version of nanoMMO, the newest first.

A version is MAJOR.MINOR.PATCH, from `project()` in `CMakeLists.txt`. While the major version is
0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes one without
changing what the client and the servers say to each other. The login server lets in a client of
its own major and minor version, and turns any other away.

## 0.86.1 (2026-11-18)

- The weather's code passes clang-tidy again: what falls and what glows get the land from `Draw`,
  which checks there is one, and the crickets' chirps step through time in a `while` loop rather
  than a `for` loop over a `float`. They sound the same as before.
- The gateway's tests no longer race. The echo server they stand in for a world with closed its
  acceptor from the test's thread while its own thread waited in `accept`, which ThreadSanitizer
  caught; now a connection of its own lets that thread go.

## 0.86.0 (2026-11-17)

- Snow falls above 19 metres in the Meadows, where the ground is white. Ash tumbles over the
  Ashlands, and in a firestorm, embers fall, burst into sparks where they land, and spread rings of
  light on the lava, while the fire lights the smoke red.
- Fireflies over the Meadows' grass at night, wisps in the Crypt, and sparks rising off the
  Ashlands' lava.
- The weather and the time of day have sounds of their own, made as the game starts: rain, on the
  ground and on the water, wind, fire, lava, crickets, birds, owls, drips, and thunder that comes
  as long after its flash as sound takes to come that far. Where there are recordings in
  `assets/sounds/`, the rain, the fire and gusts of wind are real, and going through a gate
  sounds like a teleport.
- `nanommo_cinema` films the time of day and the weather, over a player's shoulder, with the game's
  sound and music under the shot, lists a land's peaks and shores, and draws as the game does unless
  a film asks for cel shading.

## 0.85.0 (2026-11-17)

- Day and night: the server sends each land's time of day with each world, `World.sky`, worked out
  from the wall clock, and a day takes 40 minutes. The sun rises in the east and sets in the west,
  the moon lights the night, and the stars come out. The Crypt's night stays.
- Weather: the server sends each land's weather too, in spells of 6 to 15 minutes, by each land's
  odds: rain, storms, cloud and morning mist in the Meadows, ash and fire in the Ashlands, and mist
  in the Crypt. The client greys the sky under the rain and the cloud, and pulls the fog in under
  the mist. Rain falls and splashes, and wets the ground, which the sun dries, and storms throw
  lightning that lights up the sky.
- The settings' Sky page is gone. Graphics has Weather and Day and night, to turn either off.

## 0.84.0 (2026-11-16)

- A world boss: the Bone Colossus, in the Meadows at 8, -44, four and a half metres tall, level
  10, with 6000 health and a leash of twenty metres. A kind of monster with `phases` in the
  monsters file is a boss, and each phase starts as its health falls to a share of its most, with
  skills of its own and adds it calls up. The Colossus slams and sweeps from full health, throws a
  Bone Lance too from 60%, calling up three Skeletons, and a Bone Storm from 30%, calling up three
  Ash Skeletons. The content refuses phases that don't go down its health from 100%, a skill the
  kind doesn't have, and adds of a kind that isn't there, or none.
- Each monster keeps a threat table, of the health each player's blows have taken, and goes for
  whoever's taken the most, not whoever hit it last. A boss that nobody who's hurt it is left
  within its leash of goes home, back at full health and in its first phase, and can't be hurt on
  its way. Its adds go then, and fall with it as it dies.
- A boss's death is everyone's who took at least 5% of its health: each gets its experience, a
  roll of its loot and a kill for their quests. Its land hears that it's fallen, and to whom,
  thirty seconds before it's back, and as it is, on a channel of their own, `CHANNEL_LAND`, which
  the chat shows in bold. A kind's `respawn_ms` is how long it lies dead, and the Colossus's is two
  minutes.
- A bar across the top of the screen follows the boss you've picked, or one within thirty metres
  of you: its name, its health, a mark where each phase starts, and the phase it's in, which each
  world sends as `Monster.phase`. The content's hash takes in where the phases start. A tall
  monster's name is over its head, and a click anywhere on it picks it.
- The bots go for a boss within sixty metres of them before anything else, and run out of a
  narrow cone by its side. A bot swings at a monster from six metres, which the server runs it in
  from, and thinks again from where it is once it's out of a skill, so that it keeps at a big
  monster that moves. A boss stands its ground: a blow doesn't push or throw it, so a crowd can't
  shove it out of its leash.

## 0.83.0 (2026-11-16)

- Monsters' skills: big blows a monster telegraphs, in the monsters file as each kind's `skills`,
  a `CIRCLE` round it or a `CONE` in front of it, which shows on the ground for its
  `telegraph_ms`, and then lands on whoever's still in it, where it was aimed as it started. The
  Ash Skeleton's Ground Slam is a circle of three metres, for 30, after two seconds, and the Crypt
  Skeleton's Bone Sweep a cone 90 degrees wide and four metres long, for 35, after a second and a
  half. The content refuses a telegraph shorter than half a second, and a cooldown shorter than
  it.
- `Monster`'s `skill` and `lands_in` say which skill a monster's using, and how long until it
  lands, and the client draws its shape filling on the ground, in its colour, with a bar under the
  monster's health and the skill's name. The monsters' skills' shapes are in the content's hash.
- The shadows and the rings on the ground are sent to the GPU before the depth mask's back on.
- The bots run out of a monster's skill on its way.

## 0.82.0 (2026-11-16)

- Lag compensation for duels: a blow on a duel opponent is checked against where the attacker's
  client drew them, a moment in the past, and not where they are as it lands, up to 200
  milliseconds back. Each World keeps where its players have been for the last second
  (`server/history.h`), and each numbered input says when its client was drawing everyone, as
  `Input`'s `drawn`. Monsters are hit where they are.
- `nanommo_cheat`'s `stale` says it drew its duel opponent a second before, and gets 200
  milliseconds of it. `duel` says what a client does, to compare with.

## 0.81.0 (2026-11-16)

- Duels: `/duel` and a name asks a player within ten metres, and `/duel yes`, or Duel on the
  question on their screen, says yes. After a countdown of three seconds, in the middle of both
  screens, each can pick the other, as they would a monster, and hit them with a swing or a skill,
  and nobody else can hit either of them. The blow that would leave one no health leaves them one,
  and they lose, and so does one who goes more than 25 metres from the flag, where the one who
  asked stood. Nobody dies in a duel.
- `Input`'s `duel` and `target_player`, `World`'s `duel` and `Player`'s `target_player` carry them,
  and the client gets the duel through its `Link`, in a browser too.

## 0.80.0 (2026-11-16)

- The client's page is its own, `client/web/shell.html`, in place of Emscripten's: the canvas
  fills the window, and the page can't scroll, so Space, which scrolled Emscripten's page, reaches
  the game.
- `nanommo_gateway` takes browsers' pages' WebSockets on port 8090 and connects each to the login
  server (`/login`) or to a world Consul lists (`/world/<host>/<port>`) over TLS, carrying the
  game's messages as they are. It asks Consul for the worlds on a thread of its own, and logs each
  page's connection. Compose runs it, and CI builds its image.
- A page opened with `?gateway=ws://localhost:8090` logs in and plays online, with the world over
  the WebSocket; opened without, it plays offline. `Inbox` files a world server's messages for
  `Connection` and `WebSocketLink` alike.

## 0.79.0 (2026-11-16)

- `nanommo_cheat`'s `twice` plays one character on both of a world's instances, with a token for
  each, and the second saved its bag from before a trade over the trade: three swords from two. A
  character plays on one instance at a time now, the one that has their lease in Redis, and the
  other turns them away. A handover hands each lease to the next instance first.
- An instance that loses a player's lease, as one that stalls for ten seconds does, can't save them
  any more, and drops them.
- Each item that doesn't stack has a serial of its own, and each trade is on record, in MySQL's
  `trades` table, with what each side gave. The saver counts the serials that more than one item
  has, as `nanommo_duplicate_items`, and `ItemsDuplicated` fires when there are any.

## 0.78.0 (2026-11-16)

- `nanommo_client --offline` plays without the servers, in a world the client runs itself with the
  world's library, at the server's tick: the Meadows, with its monsters and NPCs, for one player,
  and nothing saved. Stories, guilds, trades and the other lands say in the chat that they're the
  servers'. A page can't reach the servers, so in a browser, the client plays offline from the
  start.
- raylib is patched (`patches/`) to hand WebGL the bones' matrices transposed, as raylib 6.2
  doesn't for an array of them, and people in a browser grew spindles.

## 0.77.0 (2026-11-16)

- The world's rules are a library of their own, `nanommo_world`, without the database or the
  network: the world, its trades, what each player is shown of it, and what their inputs do. The
  server runs its worlds with it, as before.

## 0.76.0 (2026-11-16)

- The client builds for a browser, with Emscripten: `emcmake cmake --preset web`, then
  `cmake --build --preset web`, makes a page that draws with WebGL 2. It shows the login screen,
  and says it has no way to the servers yet.
- Each shader loads through `LoadGlsl`, which gives it WebGL 2's version line in a browser.

## 0.75.0 (2026-11-15)

- `nanommo_cheat` trades with a partner, who runs `take`. `swap` moves the sword it offered once
  its partner's ready, and `run` is ready and leaves at once, each to keep the sword it gives. Both
  made three swords from two, and neither does now.
- What's offered in a trade stays where it is until the trade's over: the server won't move, use,
  wear or throw it away. The trade gives nothing unless each place still has what was offered, and
  a ready is for the offers its player saw, so one that crosses a change to an offer does nothing.
- A trade saves both players at once, as it's done, and a player who leaves is saved as they go.
  Every save into Redis is one transaction.

## 0.74.0 (2026-11-15)

- Monsters drop gold, into their killer's bag, full or not, which shows it under its slots: one to
  five for a skeleton, three to nine for an Ash Skeleton, and six to fourteen for a Crypt Skeleton.
  It's part of the bag's message, and MySQL keeps it in `players.gold`, `V12__players_gold.sql`.
- Trading: `/trade` and a name in the chat asks a player within four metres, and once they ask
  back, a window opens on both screens. Drag items onto it from the bag, type the gold, and press
  Ready, and once both sides are ready, each gets what the other offered.
- This is the simple version of trading, which trusts that what's offered stays where it was.

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
