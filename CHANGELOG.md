# Changes

What changed in each version of nanoMMO, the newest first.

A version is MAJOR.MINOR.PATCH, from `project()` in `CMakeLists.txt`. While the major version is
0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes one without
changing what the client and the servers say to each other. The login server lets in a client of
its own major and minor version, and turns any other away.

## 0.97.6 (2026-11-25)

- On Windows, the build takes `patch` and `sh` from Git for Windows, from beside the `git` it
  finds, and never another on the PATH. GitHub's Windows runners have Strawberry Perl's patch 2.5.9
  on it, which stops at raylib's patch, so CI's first Windows job failed before it built anything.
- CI's Windows job keeps what vcpkg built in GitHub's cache even when the build or a test fails, so
  the run after a failed one doesn't build the MySQL client again.

## 0.97.5 (2026-11-25)

- CI builds the game on Windows too, with Visual Studio 2026's MSVC and vcpkg's libraries, and
  runs its tests there. The runner has no GPU, so the client's tests draw with Mesa's llvmpipe, as
  they do in Docker. A deploy waits for it, as for the other checks, and in a private repository
  it's one of the night's, as Windows' minutes cost twice as much.
- The `windows` preset builds it all and runs the tests from a Developer PowerShell for Visual
  Studio, with `VCPKG_ROOT` set to vcpkg's folder: `cmake --workflow --preset windows`.
  `vcpkg.json` names the MySQL client, libsodium and OpenSSL, which vcpkg installs into the build
  folder. On Windows, CMake takes the MySQL client and libsodium from vcpkg's CMake configs, as
  there's no pkg-config, and `patch` and `sh` from Git for Windows, so the PATH needs nothing but
  Visual Studio's tools.
- The README has a badge for each build: Linux, Windows, clang-tidy, the sanitizers and the
  fuzzers.

## 0.97.4 (2026-11-25)

- The client, the bots and the gateway look up the servers' IPv4 addresses alone, as the servers
  listen on IPv4 alone. On Windows, `localhost` was `::1` first, and Windows gives up on that only
  after two seconds, so a client logged in two seconds late and a bot never got into its world. Now
  both get in at once.
- On Windows, the client keeps its settings after the first save. It writes them to a new file and
  puts that in place of the old one, which POSIX's `rename` does over a file that's there, but
  Windows' doesn't, so every save after the first failed. On Windows, `MoveFileEx` does it now, and
  the settings' test saves twice. Nothing the client and the servers say to each other changes.

## 0.97.3 (2026-11-25)

- nanoMMO builds on Windows, with Visual Studio 2026's MSVC, and its tests pass there. CMake gives
  MSVC warning flags of its own, UTF-8 sources and big object files, and builds protobuf with the C
  runtime as a DLL, as the rest of the game links it. asio's headers come from its `include`, since
  `asio/include` is a symbolic link, which Windows makes only in developer mode. Whatever brings in
  `windows.h` through asio goes without its `min`, `max`, `Rectangle`, `CloseWindow` and the other
  names raylib has too. The settings, Redis's timeouts and the cinema's pipe to ffmpeg use
  Windows' own calls where POSIX's are missing. The build runs `scripts/certificates.sh` with
  `sh`, and the civetweb patch reads the stop flag with civetweb's atomics, not GCC's. smollerGraph
  reads and writes its pages with POSIX's `pread` and `pwrite`, so Windows has no content graph,
  its tests or `nanommo_graph`.
- `.gitattributes` checks text files out with LF, Windows included. Git for Windows checked them out
  with CRLF, and in a Linux container `#!/bin/sh` with a CR after it isn't found, so `docker build`
  stopped at `scripts/certificates.sh` there. A clone from before, with no changes of its own, gets
  LF with `git rm --cached -r -q . && git reset --hard`.
- On Windows, the libraries come from vcpkg: `vcpkg install openssl libsodium libmysql pkgconf`.
  vcpkg's libmysql has no `mysqlclient.pc`, so pkg-config needs one beside vcpkg's `libsodium.pc`.
- On Windows, `localhost` is `::1` first, where the servers don't listen, and Windows gives up on a
  connection there only after two seconds. So a client logs in two seconds late, and a bot never
  gets into its world, but `127.0.0.1` works. Nothing the client and the servers say to each other
  changes.

## 0.97.2 (2026-11-25)

- Flyway 13.13.0, ClickHouse 26.10.9.12 and Pyroscope 2.4.4 in `compose.yaml`, and FrankenPHP
  1.13.6 for the website.
- The top of the repo is tidier. The tools beside the game, `cheat`, `cinema`, `bench`, `fuzz`,
  `runner` and `patches`, are in `tools/`, and the scripts, `coverage.sh`, `deploy.sh`, `doctor.sh`,
  `handover.sh`, `release.sh`, `dependencies.py` and `certificates.sh`, from `tls/`, in `scripts/`,
  so it's `scripts/doctor.sh` and `scripts/certificates.sh` now. ClickHouse's table is
  `monitoring/clickhouse/events.sql`. Nothing else works differently, but a build folder from
  before has raylib and prometheus-cpp patched already, and CMake patches them again from
  `tools/patches/`, so delete them there once:
  `rm -rf build*/_deps/raylib-* build*/_deps/prometheus-cpp-*`.

## 0.97.1 (2026-11-25)

- The client builds for a browser again. Since 0.90.0, `content/CMakeLists.txt` made the content
  graph, which links smollerGraph, in every build, and a browser's build fetches no smollerGraph,
  so Emscripten's CMake stopped before it compiled anything. The graph, its tests and
  `nanommo_graph` are the servers' and the tools' now, as smollerGraph is.

## 0.97.0 (2026-11-23)

- Factions: Concordia, in sky blue, Fortuna, in gold, and Victoria, in crimson, named in Latin
  after the Roman gods of harmony, luck and victory, each with a herald in the Meadows, in a row to
  the west of where new players start. A new character is in none. A click on a herald opens their
  window, and Enlist, and then Swear, as it asks once more, swears you to their faction, for good:
  "You've joined Concordia." Asking again, at any herald, is "You're sworn to Concordia already."
  Factions mean nothing in a fight yet, and guilds have none, so guildmates can be in different
  ones.
- `content/factions.txtpb`, with `factions.proto`, has each faction's id, name, colour and herald,
  a kind of NPC. The NPCs file has the three heralds, kinds 7 to 9, in their factions' colours.
  `ParseFactions` refuses a faction with no colour or no herald, and two with one herald,
  `CheckContent` one whose herald isn't in the NPCs file, and the content graph has a faction its
  herald `ENLISTS` players in, so one whose herald stands nowhere is out of reach. The factions are
  in `ClientHash`.
- `Input` has `enlist`, 24, a herald's id, which `World::Enlist` checks as it does a trainer, and the
  offline world takes too. `Player` has `faction`, 18, which goes with the name, once a second, and
  `Arg` has `faction`, 11, for a faction in a server's sentence. This changes what the client and
  the servers say to each other, so the login server turns a 0.96.0 client away.
- A sworn player has a diamond in their faction's colour before their level and name, as does a
  herald before theirs, and their own frame has their faction's name under it.
- A player's faction is saved with the rest of them, through Redis, and the saver writes it to
  `players.faction`, from V18, only where the row has none, so nothing switches it.
- The factions' names are translated, in the context `faction 1`, and stay as they are in Romanian,
  in the heralds' names too: "Vestitorul facțiunii Concordia". The template has 525 strings, and
  `ro.po` translates them all.
- The bots swear to a faction each, in turn by their numbers, at its herald, before their quests.
- Decision record 47 says why a faction is sworn to once, at its herald, why guilds have none, and
  why it means nothing in a fight yet.

## 0.96.0 (2026-11-23)

- What the servers say is in the player's language: a login server's errors, such as a name that's
  taken, and what a world says in the chat, its refusals, such as a bag that's full, a trade's, a
  duel's and a war's news, a guild's, what a monster dropped, a quest's progress and prizes, and a
  boss's fall, and the offline world's. Each goes as a `Message`: its English, with `$0` to `$9`,
  which is its key, as a client string's English is, and its args, numbers, names players gave,
  which stay as they are, the content's, by id, and sentences of their own, such as so much gold.
  The client translates it as it shows it, with the content's names in the player's language
  (`client/messages.h`).
- `Chat` has `message`, 4, in place of `text` for a line from nobody, and `npc`, 5, for an NPC who
  speaks, such as a storyteller, which the chat names in the player's language, over a story that
  stays English. `LoginReply` has `why`, 7, the `Message`, beside `error`, its English, which a
  client before 0.96.0 shows. This changes what the client and the servers say to each other, so
  the login server turns a 0.95.0 client away, in English.
- On the servers, `Msg` and `MsgN`, for a plural, make a sentence with its English
  (`protocol/messages.h`), and a refusal is a `Refusal`, a status with the English as its message
  and the sentence as its payload, so the logs and the tests read the English as before. A list is
  a sentence of "$0 and $1" and "$0, $1", so a language joins it its own way, and so many of an
  item is a plural of each item's own, "a Bone" and "$0 Bones", in place of an s on the end.
- `scripts/po.py extract` takes the servers' sentences from `protocol/` and `server/`, with their
  translators' comments, and a plural for each item from the items file. The template has 512
  strings, and `ro.po` translates them all, without addressing the player. The quest window's "$0
  experience" is a plural, as a quest's prizes in the chat are, and Romanian counts it in points.
- The images, packages and libraries are at their newest versions, from Dependabot's pull
  requests of 22 November: Python 3.15 for the lint stage and the storyteller, transformers 5.23.0,
  FrankenPHP 1.13.5, Guzzle 8.5.2, PSR-7 3.5.0, Laravel 13.43.0 and Tinker 3.2.0 for the website,
  Flyway 13.12.2, ClickHouse 26.10.4.207, Mailpit 1.34.0, Loki 3.7.10 and Alloy 1.22.0 in
  `compose.yaml`, and rlImGui's newest commit, hiredis 1.5.1, cpp-httplib 0.67.0 and smollerGraph
  0.20.2, whose API docs link to their code.

## 0.95.0 (2026-11-22)

- The client speaks Romanian. The settings' Interface page has Language: English, Română, and
  Pseudo, each by its own name, and the interface changes as soon as one's picked. The client
  keeps it as `Interface.language`, 6, in `settings.txtpb`: none is English.
- Translations are gettext's PO files, in `po/`: the template, `nanommo.pot`, and `ro.po`, which
  translates all of it without addressing the player, as "tu" or as "dumneavoastră". The client
  reads them itself (`Catalog`, in `client/translations.h`), with contexts, plurals and fuzzy
  entries, and asks for each string in English, through `Tr`, `TrN` for a plural, `TrLabel` for an
  ImGui label, and `N_` to mark one in a table. What isn't translated shows in English. The build
  copies `po/` next to the client, and the web build preloads it.
- A plural takes as many forms as its language has, by the PO file's `Plural-Forms`, three in
  Romanian, "1 galben", "3 galbeni", "20 de galbeni", where the client used to put an s on the end.
  The sentences with a name or a number in them have placeholders, `$0` to `$9`, which
  `absl::Substitute` fills in, in the translation's order, in place of printf's and the pieces
  `absl::StrCat` glued together. A translation with a placeholder its English hasn't got shows in
  English.
- The content's names, and the quests' words, stay English in the content and in `ClientHash`, and
  the client translates each as it shows it, by its kind and id, such as `item 8`
  (`client/names.h`). A widget keeps its English label as its ID, so a window doesn't move as the
  language changes.
- Cinzel, the titles' face, has no ș, ț or ă, so Alegreya Sans Bold is merged into it for the
  letters it lacks.
- Pseudo is made from the template: each string with its letters marked and a third longer, in
  brackets, so that one that isn't translated, or doesn't fit, shows on the screen.
- `scripts/po.py` writes the template from the client's sources and the content (`extract`), merges
  it into each language as msgmerge would, with what's changed fuzzy and what's gone obsolete
  (`update`), and checks them (`check`), which CI's lint stage runs: the template and each language
  up to date, each file readable, each plural with its language's forms, each translation with its
  English's placeholders, no `##`, and no ş or ţ for Romanian's ș and ț.
- The server's sentences, such as a login server's error or a world's line in the chat, stay
  English until 0.96.0.

## 0.94.2 (2026-11-22)

- The API docs link each class, function and file to its code, highlighted, at the line that
  defines it, and their menu links to the repository on GitHub. They have the gateway's, the
  cinema's and the red-team client's code too.
- The decision records were a list of pages after the docs, with their template among them. The
  index groups them by what they're about, on GitHub and in the API docs, where each group is a
  page under Decisions, with its records under it. Reliability and the postmortems, which the
  README lists, weren't in the API docs at all, and are now, each postmortem under Postmortems.
- The README's cover, in `.github`, wasn't copied into the API docs, so it didn't show.

## 0.94.1 (2026-11-22)

- You choose your shirt's colour as you make a character, from the six players' colours, with a
  swatch each under Hair colour, and Random picks one too. Before, the colour came from the
  character's id, which there isn't until Create, so the preview showed one colour and the
  character came out in another. `Looks` has an optional `shirt`, 5, which goes in `players`'
  `shirt`, from V17, nullable: a character from before has none, and their id still picks it.
  Adding to what the client and the servers say to each other isn't what a patch does, but
  this field is optional, and a 0.94.0 client leaves it out and draws as before.
- The bots at war wait at their side's camp, in lines of five 8 metres west (Red) or east (Blue) of
  where new players come in, until all their side is in the guild, or for a minute and a half, and
  then charge. Before, each fought as soon as it was in, where it came into the world, so a war of
  bots was one heap of them. `bench/chaos.sh war-replay`'s kills start once the guilds are full.
- How many characters an account can have in a world is one number now, `kCharactersPerWorld` in
  `protocol/limits.h`, which the login server and the client both read. Each had a four of its
  own before, so changing one would have left the client offering a character the server turns
  away, or hiding one it would make.

## 0.94.0 (2026-11-22)

- Character creation. Entering a world where you have nobody goes straight to making someone, and
  New character, under your characters while you have fewer than four there, opens it too. You
  name them, step through the builds, Slight and Broad, and the ways to wear the hair, from Short
  to Bald, and pick a skin and a hair colour from swatches, while they stand on the stage in the
  kit they'll start with, turning as you drag them. The screen starts with looks picked at random,
  so that new players don't all start out alike, Random picks again, and Create makes them.
- Looks are places in the rig's builds and in the figures file's skins, hair colours and ways to
  wear the hair, and builds and hairs have names, `Build.name` and `Hair.name`. `LoginRequest`'s
  `looks`, 13, a `Looks`, carries them. The login server has no content to count them by, so it
  keeps any up to 255 (`Characters::kMostLook`), and each client wraps each round to what its lists
  have: a look out of range draws as one there is, the same for everyone. They go in `players`'
  `build`, `skin`, `hair_color` and `hair`, from V16, nullable: a character from before 0.94.0 has
  none, and its id still picks its looks, so nobody wakes up as someone else. The bots and the
  red-team client send none.
- `CharacterListing`'s `looks`, 9, dress the characters' screen, and a world reads them from MySQL
  as the character enters it, and sends them in `Player`'s `looks`, 17, once a second with the
  name, as they never change.
- Once logged in, the worlds and the characters have no logo over them, and nothing's on the floor
  under the character picked. The buttons that do something are one height, and side by side one
  width: Enter the world and Delete, Delete and Keep, New character, Random and Create.
- Names over heads show while the pointer's on someone, or they're picked, and otherwise as the
  settings say. The settings' Interface page has "Names over heads": always over monsters, other
  players, NPCs or you, each off to begin with, and with their health, their level and a player's
  guild, each on (`Interface.nameplates`, 4, in `client/settings.proto`). An NPC with a quest for
  you shows its mark anyway. "Show the minimap", `Interface.minimap`, 5, can take the minimap off
  the screen, and the quest tracker moves up into its place. The minimap loses the plate with its
  land's name.
- O opens the settings, and H the help, with a page each for moving, fighting, items, quests,
  guilds and duels, chat, and the keys, which are the settings' list. The bag's count of slots used
  is gone, and its line on throwing things away and the map's on clicking to walk are in the help.
  The buttons in the bottom right are round, like the chat's, with nothing behind them, and have
  one for your quests and one for the help. The character window loses the ring of light on its
  floor, and the name, level and land under the figure.
- T opens your quests: those you're on, with a tick by each you've killed enough for, and those
  you've done, the highest id first, each with who gave it, what it asks for, how far you've got
  and what it gives. Give up asks whether you're sure, as what you've killed for it goes too, and
  sends `Input`'s `give_up`, 23, with the quest's id. The world takes it off you
  (`World::GiveUp`), and its giver offers it again. A quest you've turned in stays done.

## 0.93.0 (2026-11-21)

- Crafting, wherever you are. K opens the crafting window, with the recipes of your level and
  below, what each takes, as how many you have in your bag of how many it needs, what it makes and
  in how long, and a Craft button. A recipe takes from the bag, never from what you wear, nor from
  what you offer in a trade, and takes its seconds, with a bar over the hotbar. Moving, a swing or
  a skill, a blow, dying, a trade that opens, a duel you say yes to and leaving stop it, and you
  lose nothing. Once it's done, the world checks the bag again, and takes what it takes and gives
  what it makes, as one change to the bag, saved once.
- Recipes are content, in `content/recipes.txtpb`: twelve bones carve a Bone Buckler, from level 1,
  the sword and ten bones bind a Bone Sword, with 18 damage, from level 2, and a Bone Sword and
  twenty bones temper an Ash Blade, from level 4. The two new items are the Bone Buckler and the
  Bone Sword.
- The content graph needs all a recipe takes before a new player reaches it, in rounds that add
  `CRAFTS` until one adds nothing, and `Cycles` refuses an item that goes back into itself through
  recipes. `nanommo_graph` prints both.
- `Input`'s `craft`, 22, starts or stops a recipe, three at once and then one a second, and `World`'s
  `crafting`, 29, says what you're making and how long you've been at it. The recipes are in
  `ClientHash`.
- Each thing made is an event for the analytics, `craft`, with the item and how many.

## 0.92.0 (2026-11-21)

- A war's score counts each kill once. Each kill has an id of its own, and the saver inserts it as
  a row of `war_kills`, from V15, with `INSERT IGNORE`, so a batch it reads again, as one MySQL
  committed while Redis hung, adds nothing. A war's score is how many of its rows each side has.
  `guild_wars`' `challenger_kills` and `challenged_kills` aren't written any more, and stay for
  0.91.0's servers.
- A player scores for a war once every five minutes at most, whoever kills them, so that friends
  on either side can't farm a war's score. The kill still happens.
- While a guild is at war, nobody leaves it, nobody's removed from it, it can't be disbanded, and a
  character in it can't be deleted, until the war's over.
- The worlds count the war kills they hand the saver, `nanommo_war_kills_total`.
- `nanommo_bot` takes `war` after its transport, which sends half the bots into each of two
  guilds, Red and Blue, whose first bots found them, take them to war and ask the rest in. The two
  sides fight each other.
- `bench/chaos.sh war-replay` has Redis hang once the saver has a batch of war kills, and compares
  the kills the worlds counted with the scores. Before the fix, three runs out of three counted
  some twice: 196 kills scored 200, 264 scored 265, 267 scored 281.
- The red-team client's `farm` and `feed` farm a war's score: fifteen kills scored 15 to 0 in six
  minutes before, and seventeen scored 2 to 0 in six and a half after.

## 0.91.0 (2026-11-21)

- Guild wars. A guild's leader asks another guild in the world to war with `/war` and its name,
  and the other's leader asking back is a yes. For half an hour, the players of the two can hurt
  each other anywhere, and their blows kill, with the ten seconds' wait, and give no experience and
  drop nothing. A guild is in one war at a time, until its time is up. Wars are in MySQL, in
  `guild_wars`, from V14, and the asks in Redis, for a minute.
- The enemy guild's players have red names over their heads, and a click picks one as a duel
  opponent is picked. A skill that hits all around hits every enemy it reaches. The guild window
  shows the war: the enemy, the kills both ways and how long it has left.
- Each kill is a line in both guilds' chat, and goes into Redis's `war_kills` stream with the tick's
  save. The saver adds the kills to their wars' scores in MySQL, which the worlds read back every
  five seconds, as they do the wars.
- The protocol has `GuildAction`'s `war`, 11, and `Guild`'s `war`, 4, a `GuildWar`.

## 0.90.2 (2026-11-21)

- Walking diagonally is smooth again. The server walks a diagonal no faster than a straight line,
  but the client walked you with W and D as they were, about 41% faster, before the server
  answered, so each world it sent pulled you back. `WalkAlong`, which both use, keeps the direction
  to a length of one now, so the client's guess and the server come out the same.
- The windows' panels lose the gems at their top corners, as the website's did.

## 0.90.1 (2026-11-20)

- The game builds with its own httplib again, 0.65.0, set up for ThreadSanitizer. smollerGraph's
  `sg` names httplib too, at 0.60.1, and was declared first, so its httplib won, and looked names up
  on a thread of glibc's that ThreadSanitizer can't follow, which crashed `Tracing.PostsToACollector`
  under it. smollerGraph is now declared after our httplib.
- The content graph's code passes clang-tidy: `Relationship` checks both names are in the graph, and
  `Names` and `Unreachable` check each cell has a value, before they use it.

## 0.90.0 (2026-11-20)

- The content is checked as a graph, in smollerGraph, the graph database from its own series, which
  the build fetches at v0.20.0: the content tests fail when a new player can't get to a land, a
  monster, an NPC, a quest, an item or a skill, or a quest's monster is out of reach.
  `nanommo_graph` writes the graph for smollerGraph's `sg` to serve, with its explorer.

## 0.89.0 (2026-11-18)

- Redis and MySQL answer in time or count as down: 250 milliseconds for each Redis reply, a second
  to connect to MySQL and three for each read or write, and after a timeout, neither is asked again
  for a moment, a second for Redis, five for MySQL. A world that's more than a second behind lets the
  ticks it missed go, rather than running them back to back.
- The saver keeps each batch in Redis until MySQL has it, so a saver that dies mid-flush loses
  nothing.
- A trade that Redis can't save is called off: both sides leave, unsaved. Before, the one who stayed
  kept what they were given, and the one who'd left kept it too.
- A login gives up after five seconds, `kLoginDeadline`, in the client, the bots and the red-team
  client. The bots count their logins by result, `nanommo_bot_login_duration_seconds`, and the login
  server lists the worlds from the last half a minute when Consul doesn't answer.
- The first postmortem, in `docs/postmortems/`, of a chaos run that dropped every player in Ember.

## 0.88.0 (2026-11-18)

- Service level objectives, with error budgets: 99.9% of each world's ticks within their 50
  milliseconds, 99% of logins answered within five seconds, and MySQL no more than 30 seconds behind
  Redis 99.9% of the time. Prometheus records each every 30 seconds, and alerts on how fast a
  budget burns, over an hour and six hours, as a page, and over three days, as a ticket:
  `TicksBurningBudget`, `TicksDrainingBudget`, `LoginsBurningBudget`, `SavesBurningBudget`. A
  dashboard, nanoMMO reliability, shows each budget left this month and how fast it's going.
  Prometheus keeps 31 days, on a volume of its own.
- The login server serves metrics on port 9467: how long it took to answer each request, by what
  came of it, `nanommo_login_duration_seconds`. The saver's `nanommo_saver_flushed_timestamp_seconds`
  is when its last good flush ended, and `nanommo_unsaved_players` how many wait. The worlds count
  the players they drop without saving them, `nanommo_players_dropped_total`, by why.
- `bench/chaos.sh`, chaos experiments in staging, one fault at a time, while bots play: Redis or
  MySQL frozen, stopped or slow, the saver killed mid-flush, the login server frozen, Consul stopped,
  Alloy frozen. It reports what the metrics, logs and traces saw as JSON, and refuses a deployment's
  stack.
- The bots log in again after losing their world, with a backoff that doubles, with jitter.

## 0.87.0 (2026-11-18)

- Distributed tracing. The login server, the worlds and the saver trace the work they do for a
  request, and send the spans in OpenTelemetry's protocol, OTLP, as JSON over HTTP, to the collector
  `NANOMMO_OTLP_ENDPOINT` names: Alloy, in Compose, which keeps the traces worth keeping and sends
  them to Tempo, at http://localhost:3200. Without it, a server traces nothing. The tracer is the
  servers' own, `server/tracing.h`, with W3C Trace Context's `traceparent`.
- A request to the login server is a trace, with checking or hashing the password a span of its
  own. Entering a world is one trace across the login server and the world, as the token's record
  in Redis carries it, and so is a handover across the two instances of a world. A trade is one
  trace from the world, through the trades stream, which carries it too, to the saver. Each call to
  Redis, statement to MySQL and request to Consul made for a request is a span, which names what it
  asks, and never its keys or values. The tick isn't traced, nor the client, nor going through a
  gate.
- The lines the servers print for a traced request end with its trace's id, `trace_id=`, which
  opens the trace from Grafana's logs, and a trace opens its lines. Alloy keeps every trace with an
  error, every one over 100 milliseconds, and a tenth of the rest.
- Grafana has Tempo as a data source, a dashboard of traces, nanoMMO traces, with who calls whom,
  and exemplars that open a metric's trace. Prometheus takes the metrics Tempo makes from the spans
  with its remote-write receiver, and anonymous visitors to Grafana can edit, as Explore is an
  editor's. The worlds and the saver count the spans they drop, `nanommo_spans_dropped_total`.
- A MySQL connection keeps the reads it's prepared, and runs one again in a round trip rather than
  two. The traces found it: with MySQL two milliseconds away, a player's hello took 19 milliseconds
  of the world's tick thread, and takes 9.9 now.

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
