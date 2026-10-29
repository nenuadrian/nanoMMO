# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

You need CMake, Ninja and a C++20 compiler, and three libraries from your system's package manager:
MySQL's client library, libsodium for hashing passwords, and OpenSSL for TLS. The client draws with
raylib, on top of the system's OpenGL.

- macOS: `brew install mysql-client libsodium openssl@3 pkgconf`.
- Ubuntu: `sudo apt install pkg-config libmysqlclient-dev libsodium-dev libssl-dev xorg-dev
  libgl1-mesa-dev`, where the last two are the X11 and OpenGL headers.

CMake fetches and builds the other libraries: GoogleTest, raylib, Dear ImGui, rlImGui, abseil,
protobuf, Asio, prometheus-cpp, hiredis, nlohmann/json and cpp-httplib. The first build compiles protobuf, so it takes a few
minutes.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The servers keep accounts in MySQL, and hand logins over through Redis. The game server saves the
players into Redis, and a saver takes them on to MySQL (see [Saving players](#saving-players)).
Each game server is a world, which registers with Consul, and the login server lists the worlds
from there (see [Worlds](#worlds)). Docker Compose runs MySQL, Redis, Consul and the saver, and
before them Flyway, which makes the tables from the migrations in `db/` (see
[Migrations](#migrations)). The clients talk to the servers over TLS, with certificates from an
authority of our own, which `tls/certificates.sh` makes the first time (see [TLS](#tls)):

```bash
docker compose up -d --build mysql redis consul saver
tls/certificates.sh
./build/bin/nanommo_login
```

Then, in another terminal, the game server, and in one or two more, the client:

```bash
./build/bin/nanommo_server
./build/bin/nanommo_client
```

The client opens on the worlds that are up, with how many players each has. Pick one with the arrow
keys and Enter, or a click, and F5 or Refresh asks for the list again. Then type a name and a
password, and Create account the first time, and Log in after that. The client asks the login
server on `localhost`, or on the host you give it (`./build/bin/nanommo_client 192.168.1.20`), and
connects to the world at the host and port that world registered. It gets the world over UDP, unless you put `tcp` after the
host (`./build/bin/nanommo_client localhost tcp`); see [Packet loss: TCP and UDP](#packet-loss-tcp-and-udp).
Each player is an animated person, in their own skin and hair, and in a shirt of their own colour,
with a sword, on an island of hills, woods and beaches, with its level, name and health over it, or
a cube until you've made the characters (see below).
Skeletons wander around the middle, and in camps around the island, and a few NPCs stand near the
middle, each a person with its name over it (see [NPCs](#npcs)). Click the storyteller, in purple,
to ask it for a story (see [The storyteller](#the-storyteller)), the trainer, in grey, to learn
skills (see [Learning skills](#learning-skills)), and the guard, with a gold mark over its name, for
a quest (see [Quests](#quests)). W, A, S and D, or the arrow
keys, move yours, whose name is in gold, Shift makes it run, Space swings the sword, and 1 to 4 use
your skills, from the bar along the bottom (see [Skills](#skills)). I, or View, Bag, opens your bag,
where a new player has three health potions, and wears a shirt, trousers and shoes (see
[Items](#items)).
The camera follows you: Q and E, or dragging with the right mouse button, turn it around you,
dragging up and down tilts it, and the mouse wheel zooms in and out. W always walks away from the camera. The frame in the top left
corner shows your health, and your experience towards the next level. Enter opens the chat, in the
bottom left corner, where Enter again says what you typed to everyone in your world (see
[Chat](#chat)).
Press C, or choose View, Character, to open your character screen. It shows your name, level,
health, experience and position, next to an idle preview of your character facing the camera, and
what you wear. The client renders that preview into a texture with a camera of its own, then ImGui
draws the texture in the window. View, Settings changes the sound, and the sky, which is the colour of the fog too, and Game, Log out
takes you back to the login screen. Next time you log in to that world, you're where you left off.

`protocol/nanommo.proto` is the contract between the client and the servers. The build runs protoc on
it, and all sides use the C++ it generates. Items, monsters and NPCs are data (see
[Content](#content)).

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.

## Checking the code

The tests check what we thought to test. Four more checks look for what we didn't. In a public
repository, where GitHub's runners are free, CI runs each of them on each push. A private one pays
for its minutes, so a push there gets the tests, and clang-tidy, the sanitizers and the fuzzers run
once a night, or when started by hand (`gh workflow run CI`). The coverage report, which only a
public repository publishes, isn't made there at all. Either way, a check that passed on the same
tree doesn't run again, as after a rebase that moves commits but not code, and a newer push to a
branch cancels the checks still running for the one before.

Each stage of the Dockerfile that compiles does it through ccache, which keeps what the compiler
made in a cache mount that outlives the build, so protobuf and the other libraries compile once,
and a build after a change compiles what changed. Each build prints how much it found there
(`ccache --show-stats`). The mount lives in Docker's build cache, so it stays on your machine;
CI has a fresh one for each job, and moves the job's ccache in and out of GitHub's cache around
its builds.

The sanitizers build checks into the code, which catch it doing something wrong as it runs.
`-DNANOMMO_SANITIZE=address,undefined` builds everything, the libraries too, with AddressSanitizer,
for reads and writes out of bounds or after a free, and leaks, and UndefinedBehaviorSanitizer, for
signed overflow, bad shifts and casts and the like. Both stop the program at the first one.
`-DNANOMMO_SANITIZE=thread` builds it with ThreadSanitizer instead, for data races, which it
reports as it goes. The code runs about twice as slowly with the first two, and five to fifteen
times as slowly with ThreadSanitizer. The `sanitize` stage builds with Clang and runs the tests:

```bash
docker build --target sanitize .
docker build --target sanitize --build-arg SANITIZE=thread .
```

ThreadSanitizer keeps its own record of memory at addresses that clash with the kernel's when the
kernel randomizes them with more than 28 bits. Docker Desktop's kernel uses 18, and Ubuntu's 32 or
33, as on GitHub's runners or in a Colima VM. There, lower it first, once after the machine starts:

```bash
docker run --rm --privileged alpine sysctl -w vm.mmap_rnd_bits=28
```

ThreadSanitizer sees threads start, and each lock and atomic in the code it built. Under it, httplib
looks names up without the thread glibc starts for that, and `tsan.supp` leaves out what Mesa's
library does with its threads, as Mesa draws the client's tests in Docker and orders its threads in
code ThreadSanitizer can't follow.

`compose.sanitize.yaml` runs the login server, the world server, the saver, the bots and the tests
from the `sanitize` stage, so that the bots play against servers built with the sanitizers. What the
sanitizers find goes to their logs:

```bash
export COMPOSE_FILE=compose.yaml:compose.sanitize.yaml
docker compose up -d --build login server saver
docker compose up -d bots
docker compose logs server bots
```

`SANITIZE=thread` in front of the `up` builds them with ThreadSanitizer, and `docker compose run --rm
tests` runs the tests against MySQL, Redis and Consul with the sanitizers on.

The fuzzers in `fuzz/` run with libFuzzer, which comes with Clang. Each is a function that takes
bytes, and libFuzzer calls it with input after input, made from the inputs that reached code the
others didn't, with the sanitizers on. `stories_fuzzer` hands the storyteller's lines whatever a
model might write, and checks that each line fits in the chat, whole characters and all.
`world_fuzzer` plays the game with whatever a client might send, and checks that each player stays
somewhere real, with health between nothing and their most, that their bag holds no stack
bigger than it can, in a slot that's there, and wears each item in its slot, and that each quest
they're on is one the content has, with no more kills than it asks for, and not one they've done.

```bash
docker build --target fuzz -t nanommo-fuzz .
docker run --rm nanommo-fuzz build/bin/stories_fuzzer -max_total_time=60
docker run --rm nanommo-fuzz build/bin/world_fuzzer -max_total_time=60
```

When a fuzzer breaks a check, it stops and prints the input that did it, and saves it to a file
named `crash-` and its hash, which the fuzzer runs again when given that file.

clang-tidy reads the code for bugs that compile: `.clang-tidy` turns on its checks for likely bugs,
the static analyzer, and its checks for threads and for performance, less a few that flag what we
mean to do, and makes each finding an error. `docker build --target tidy .` runs it on the code,
without the tests.

Last, the coverage report shows which lines the tests run, and how often. `docker compose run --rm
coverage` builds the code with Clang's counters, runs all of the tests against MySQL, Redis and
Consul, and writes the report to `site/coverage`. It reads the servers, the bot and the client too,
so that code no test reaches counts, at zero. CI publishes it with the API docs, at
[nenuadrian.github.io/nanoMMO/coverage](https://nenuadrian.github.io/nanoMMO/coverage/), from a
public repository only, as Pages would put a private copy's code where anyone can read it.

`patches/` holds fixes to the libraries we fetch that their releases don't have yet, which CMake
applies as it fetches them: civetweb's for a data race on the flag that stops its threads, which
ThreadSanitizer found.

## Characters

The players are people, posed by Mixamo's X Bot and Y Bot, with an idle, a walk, a run, a sword
slash and a death, and a clip for each skill that has one: a thrust, a kick and a spin.
Mixamo's terms don't let us share its files, so they're not in the repo:
[assets/mixamo/README.md](assets/mixamo/README.md) lists what to download from mixamo.com. Then make
the characters:

```bash
docker build --target characters --output assets/characters .
```

The `characters` stage converts the FBX files to glTF with FBX2glTF, and
[assets/tools/merge.mjs](assets/tools/merge.mjs) puts each character and the clips into one `.glb`
that raylib can load: one skeleton, the walk and the run in place, and a tenth of the triangles. The
slash and the death keep their motion, so a player falls back as it dies. The build copies
`assets/characters` next to the client, which loads them from there. Without them, the players are
cubes, and the client's character tests skip themselves.

The client draws neither bot's mesh. A player's figure is made of parts, in
[content/figures.txtpb](content/figures.txtpb): limbs with round ends, from one joint of the rig to
another, and low-poly balls at a joint, each in metres, along the figure's own axes as it stands in
the rig's T-pose. A part on the left can be mirrored onto the right. The client builds the parts'
triangles where the bones rest, and binds each corner to its part's bone alone, so the clips pose the
parts as they posed the mesh, rigidly, like a jointed doll. The body's parts are white, which takes
on the player's skin, with dark eyes. The file lists the skins, the hair colours and the ways to wear
the hair, and each player's id picks one of each, the same on every client. An item's look can have
parts too, which a player who wears it has over their figure, in their own colours, or for a dyed
one, such as the Linen Shirt, in the player's colour (see [Items](#items)). Each player in the
world's message says what they wear, so everyone sees each other in it. NPCs are people too, but
their kind in the NPCs file says their skin, hair and clothes, which a player's id doesn't pick, and
which of the two bots they're on (see [NPCs](#npcs)). With cel shading on, the
outline is drawn with each figure's hull, the same parts with smooth normals, so it stays whole
around the parts, but for those flush with a bigger one, such as the eyes. Nothing casts a shadow,
so each figure has a soft dark patch under it, on the ground.

Each player holds the weapon in their main hand (see [Items](#items)), so far Quaternius's sword, from
[Poly Pizza](https://poly.pizza/m/ajOJ2NLz5m), which is public domain and in the repo, as it was
downloaded, in `assets/weapons`. The client scales it to its length in the file, a metre, holds it
where the file says, a sixth of the way up, and, each frame, puts it where the right hand's bone is
in the pose: along the knuckles, with the blade out past the index finger. glTF colours are linear,
so the client turns the sword's into sRGB, as our shader writes colours out as they are. An item's
tint goes over them, so the Ash Blade is the sword in another colour.

The server decides which way each player faces, and whether it stands, walks or runs. It moves them
at 1.8 and 3.1 metres a second, the speeds of the walk and the run, so that the feet don't slide.
The client blends from one clip into the next over a quarter of a second, and smooths each player's
position between the server's ticks.

## Music

While you're in the world, the client plays "Background Music" by
[Pavel Bekirov](https://pixabay.com/users/paulyudin-27739282/), from [Pixabay](https://pixabay.com/).
Pixabay's licence lets us use it in the game, but not hand out the file on its own, so it isn't in
the repo: [assets/music/README.md](assets/music/README.md) says where to get it. The build copies
`assets/music` next to the client.

- `client/audio.h` has raylib's audio device, which plays through the system's sound with
  miniaudio, opened after the window, and the track, which raylib streams from its file. Without a
  sound card, as in a container, or without the file, the client plays on in silence, and says why as
  it starts.
- A stream plays from a buffer of two halves, about a quarter of a second each: while the device
  plays one half, `UpdateMusicStream` decodes the next part of the file into the other. The client
  calls it each frame, so a frame would have to take a quarter of a second for the music to skip.
  Without it, the buffer stays empty, and the music makes no sound.
- The track fades in over three seconds as you enter the world, and plays through. Then it rests
  for 45 seconds, as an hour of one track on repeat wears thin, and plays again. Leaving the world
  fades it out over a second and a half. `client/jukebox.h` decides all of that from the frame's
  time, whether you're in the world and whether the track is playing, apart from raylib, so that its
  test needs no sound card. raylib loops a music stream by default, so the track turns that off.
- View, Settings has the sound: Volume, which raylib's master volume applies to each sound the game
  makes, so far the music, Music, and Mute, with the track's credit under them. Each slider goes
  through 40 decibels, from a tenth of the amplitude at the middle to full, and its bottom is
  silence, as we hear loudness on a logarithmic scale: a slider that moved the amplitude in a
  straight line would do nearly all of its work at its bottom end.
- The client keeps the sound settings between runs, in `settings.txtpb`, in protobuf's text format
  like the content, in `~/Library/Application Support/nanoMMO` on macOS, and in the XDG config
  folder elsewhere, `~/.config/nanommo` unless `XDG_CONFIG_HOME` says otherwise. Each field is
  optional, so a setting the file doesn't have keeps its default, and with no file, as on a first
  run, they're all the defaults. A player can edit the file by hand, so a volume in it stays between
  nothing and full, as one past full would play many times louder, and one that isn't a number keeps
  its default. The client writes the file once a slider lets go, to `settings.txtpb.new` first, and
  renames that over the old file, which replaces it in one step, so a crash halfway through leaves
  the old file whole.

## Post-processing

The client draws the world into a frame of its own, a render texture, and puts the frame on the
screen with the GUI on top, as raylib's
[post-processing example](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_postprocessing.c)
does, so that shaders can work on the whole picture first. `client/postprocess.h` has it.

- OpenGL keeps a texture's rows from the bottom up, so the frame goes to the screen with a negative
  height, which flips it the right way up.
- The sea and the glows are see-through, and leave the frame see-through where they are, so the
  shader that puts it on the screen writes it opaque: blending it in would let an older picture show
  through.
- The frame is the screen's size in pixels, from `GetRenderWidth` and `GetRenderHeight`, and goes to
  the screen fitted to its points. A high-DPI screen has more pixels than points, so a frame in
  points makes the world coarse. A render texture has one sample to a pixel, so the window doesn't
  ask for multisampling, which would smooth nothing but the GUI: on a high-DPI screen, a jagged edge
  is a pixel wide.
- Bloom is the light that spills around what's brighter than the screen can show. The frame keeps
  half a float to each channel, where raylib's render textures keep eight bits, which stop at white,
  so the world's colours can go past it. The bloom keeps what's past white, in a frame half the
  screen's size, halves that four more times, down to a thirty-second of the screen, and adds the
  halves back up from the smallest, each spread over the nine pixels around as it doubles. A quarter
  of it goes on top of the frame. raylib's example blurs the whole picture onto itself instead, which
  washes it out.
- The lava, the ooze and Second Wind's glow give off light of their own, their emission, which the
  shaders add to the sun's, the lava's and the ooze's most at the tops of their waves, and which
  takes them past white, so they bloom. A glow adds its light to
  what's behind it. The screen cuts each channel at white on its own, which turns a bright orange
  yellow, so a colour past white goes to the screen scaled down to white as a whole, by its brightest
  channel.
- Cel shading, from raylib's
  [cel shading example](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_cel_shading.c):
  the sun's light comes in three flat bands, in shadow, half lit and lit, as a cartoon's colours do.
  The light is worked out for each pixel, from the normal the vertex shaders hand on, so that the
  bands follow the surface.
- Cel shading's outlines, the way raylib's example draws them: each figure is drawn a second time
  first, inside out, four centimetres bigger along its normals, in a dark colour that fades into the
  fog, so that its back shows around the figure's edges. Our characters are posed on the GPU, so
  their outline has a shader with bones of its own. The ground and its trees have none.
- The frame has one sample to a pixel, so the world's edges are smoothed with FXAA, Timothy Lottes'
  fast approximate anti-aliasing, as the last pass: the frame and its bloom go into a picture of
  eight bits a channel at the screen's size, and a shader finds which way each edge runs from the
  brightness around each pixel, and blends along it, a pass over the screen's pixels.
- View, Settings has the graphics: Bloom, on by default, Cel shading, off by default, as a cartoon's
  look is the player's to choose, and Anti-aliasing, on by default. The client keeps them with the
  sound in `settings.txtpb` (see [Music](#music)).

## The world

Each land's island comes from a seed, a number: the game server's `NANOMMO_WORLD_SEED`, 1 by
default, plus the land's own (see [Lands](#lands)). The server sends it to each client with the
client's own id, and both make the same island from it, in `terrain/`, so the island itself never
goes over the network.

- The ground is a grid of cells two metres across, 256 of them along each side, so 512 metres, with
  two triangles in each cell. Each corner's height is value noise: a random number at each whole
  number, from a hash of the seed and the point, and smooth between them. Five octaves of it, each
  twice as fine and half as strong as the one before, make the hills, and four more, larger, make
  the mountains. Towards the edge, the land sinks into the sea, and in the middle, where the players
  start, it's gentle and above the sea.
- A player stands on the triangles, as the client draws them, not on the noise between the corners.
- The trees grow on a grid of four-metre squares, at most one in each, between the beach and the
  rock, and not within sixteen metres of the middle: in woods where the noise is high, and a few on
  their own elsewhere.
  Each square's hash says where in the square its tree stands, which way it faces and how big it is.
- The server lets players and monsters walk anywhere on the island, and into the sea up to their
  knees, 40 centimetres, but not through a trunk. Running into the sea or a tree, they slide along
  whichever way they still can. A player saved somewhere that's sea now starts in the middle.

The server and the clients have to make the same heights and trees to the last bit: the server
decides where the trees are in the way, and the client draws them. Compilers for Arm, and for x86
with `-march` options that have FMA, fuse a multiply and an add into one instruction, which rounds
once instead of twice, so the same code would make a slightly different island on a Mac than on an
x86 server. `terrain/CMakeLists.txt` turns that off with `-ffp-contract=off`, and the noise uses
no `std::sin` or `std::pow`, whose results differ between standard libraries. The terrain test pins a
fingerprint of the whole island, its heights and its trees, so a machine that makes another island
fails it.

The client colours each face by its height and slope: sand on the beaches, grass, rock on steep
slopes and the high ground, and snow on the peaks, each face a little lighter or darker than its
neighbours. The triangles share no corners, so each has the normal of its face, and the light shades
each face flat.

The sea has a shader of its own, in `client/sea.h`: a square of the ground's cells around the
camera, 320 metres across, each cut in two the way the ground's are, which follows the camera a
whole cell at a time. Three waves, each its own way and length, and each as fast as a wave of its
length at sea, raise and lower its corners in the vertex shader, and the fragment shader lights each
triangle flat, with the normal of its face, worked out from how far the world moves from one pixel
to the next, so the sea is facets, like the ground. The vertex shader reads the ground's height
under each corner from a texture of floats, one a corner: the waves lap at the shore, the shallows
are lighter, and the ground shows through them, and the line where the sea meets the ground has a
colour of its own. Past 40 metres the waves calm down, as cells a few pixels big only flicker, and
towards the edge of the square the sea fades into the sky. Each land's look gives its sea a liquid:
the Meadows' water, with foam at the shore and the sun's glint, the Ashlands' lava, thick and slow,
which cools to a crust at the shore and whose crests glow, and the Crypt's green ooze.

The colours are the land's look, in `client/atmosphere.h`: those of the ground, the sea and the
trees, which go into the meshes, and an atmosphere, which the shaders read each frame. The
atmosphere is the sky, which the fog fades into, where the fog starts and where it hides everything,
which way the sun is, the light it adds to a face turned to it, and the light a face gets even when
it's turned away. In the Meadows, a warm sun shines from the side and low, so the faces of the hills
and trees stand apart, and the fog goes from 45 metres to 150. The shaders work out the fog for each
pixel, from how far it is, so the far sea fades too. View, Settings changes the
atmosphere as you play, with presets for dusk, night, mist and ash, and Reset puts the land's back.

The tree is Poly by Google's, from [Poly Pizza](https://poly.pizza/m/6pwiq7hSrHr), under
[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/), in the repo, as it was downloaded, in
`assets/trees` (see [assets/trees/README.md](assets/trees/README.md)). Its texture is a palette of
four colours, so the client reads the model as triangles, and gives each face the colour under its
middle.

### Drawing the island fast

The client cuts the island into chunks, squares of 32 metres, and puts each chunk's ground and trees
into one mesh, with the trees moved into place. Each frame, it draws the chunks that are closer than
where the fog hides everything, 150 metres in the Meadows, and inside the camera's view, and no others. A chunk is out of view when all
eight corners of its box are past the same side of the screen, or nearer than the near plane, or
past the far one.

`nanommo_client_bench` measures it. It walks once around the middle of the island, 80 metres out,
with the camera turning to look over the player at the middle, drawing the same frames each time,
as fast as it can, and then prints the frames per second, the median and 99th percentile frame time,
and the draws and triangles each frame. It takes the number of frames, 1200 by default, the seed,
and a file to write the numbers to as JSON too:

```bash
./build/bin/nanommo_client_bench
./build/bin/nanommo_client_bench 1200 1 bench/results/frames.json
```

Build it as Release (`-DCMAKE_BUILD_TYPE=Release`) to measure. With a draw for each tree and the
ground as one mesh, it took 1518 draws and 369,086 triangles a frame, and 1.7 ms a frame at the
median on an M4 Pro. With the chunks, it's 20 draws and about 62,000 triangles, and 0.25 ms. In
Docker, with Mesa's llvmpipe drawing on the CPU, it went from 14.8 ms to 13.0 ms: on the CPU, most
of the time goes on the pixels, not the triangles.

```bash
docker build --target build -t nanommo-build .
docker run --rm nanommo-build xvfb-run --auto-servernum -s "-screen 0 1280x720x24" \
  ./build/bin/nanommo_client_bench 300
```

## Monsters, health and levels

The monsters are Quaternius's skeleton, from [Poly Pizza](https://poly.pizza/m/wODZYCgX5Z). It's
public domain (CC0), so it's in the repo, in `assets/monsters`, as it was downloaded:
[assets/monsters/README.md](assets/monsters/README.md) says where it's from. The client loads it as
it is, for each kind of monster whose look names it. It scales it to the kind's height, finds its clips by the word in their names, and walks with its
run, played at 0.4 of its speed, which puts the feet at the walking speed. It reads the bind pose
from the file's inverse bind matrices, as raylib binds the mesh to where the bones rest, and the
skeleton's arms rest about 70 degrees from where its mesh was bound.

The server spawns the monsters that `content/monsters.txtpb` lists (see [Content](#content)): three monsters
around the middle of each land, and up to three more at each of eight camps, which the land's seed
places between the beach and the rock, at least thirty metres from the middle. It runs their AI each
tick: a monster rests for two to six seconds, walks to a random place within its kind's wander of where
it spawned, at 1.8 metres a second, and rests again. If the sea or a tree stops it, it rests, and
picks another place. The world's random generator takes a seed, so the tests pick theirs.

## Fighting

Space swings your weapon at the nearest monster within its reach, and turns you to face it. The
sword's numbers are in the items file: it reaches 1.6 metres, and a swing takes two seconds, the
length of the slash animation, and you stand still until it's over. The sword hits 0.9 seconds in, as
it comes across the front, if the monster is still alive and in reach, for fifteen health, and each
level after the first adds five to whatever you hold, and what you wear adds its damage. With no
weapon in your main hand, Space does nothing, and putting the weapon away ends a swing. Holding Space down swings again and again. All
of it happens on the server: the client only says that Space is down, in `Input`.

A monster goes for the nearest living player within its kind's sight, three metres for a skeleton, if that player is within
its leash of the monster's home, four metres. A monster that's hurt goes for whoever hurt it last
instead, seen or not, while they're within its leash, so that a kick that knocks it out of sight
doesn't make it forget them. The content's checks keep each fixed home further than that from the middle of the
island, so the middle, where players start and come back, is safe. A monster runs at them at the
players' running speed, and swings once it's within 1.2 metres. The skeleton's swing takes 0.9 seconds, its attack animation, and hits for five,
0.65 seconds in, less the armour you wear, down to one. An Ash Skeleton, in the Ashlands, is level two, and hits for eight. Run, or die, and it gives up and wanders back.

A monster with no health left dies, and whoever killed it gets its experience, fifty for a skeleton, and ninety for an Ash Skeleton,
and what it drops, into their bag (see [Items](#items)).
Going up a level fills your health, to the new level's most. A player with no health left dies. The
dead play their death animation, lie there for ten seconds, and come back: a monster where it
spawned, and a player in the middle of the island, both at full health.

Players and monsters have a level and health, which everyone sees. A player's experience is its own:
the server works out its level, and sends its experience into the level, and what the next level
takes, to its client alone, in the message with its id that goes after the world, or with each part
over UDP. Each level takes a hundred more experience than the one before, and a player has a hundred
health at level one, and twenty more for each level after it, and what they wear adds its health.

Experience and health are saved with where you are (see [Saving players](#saving-players)), so you
come back with the health you left with. If you leave dead, you come back as the respawn would bring
you back, in the middle at full health. A player saved before health was kept joins at full health.

## Skills

A skill is a move with a cooldown. Each player knows the thrust from the start, and learns the
others from a trainer, from their levels on (see [Learning skills](#learning-skills)). The four in
`content/skills.txtpb` are a skill a level, from one to four:

| Skill | From level | Cooldown | What it does |
|---|---|---|---|
| Thrust | 1 | 8 s | 35 damage to the nearest monster within 2.4 metres |
| Kick | 2 | 6 s | 10 damage, and knocks the monster 3 metres back |
| Second Wind | 3 | 30 s | Heals 40, up to your most |
| Whirlwind | 4 | 12 s | 25 damage to each monster within 2.5 metres |

Each level after the first adds five to a skill's damage, as it does to a weapon's. There's no
skill tree: a trainer teaches each of its skills to any player who has reached its level.

The hotbar along the bottom of the screen has the skills in the order of their levels, each on a
number key, 1 for the first, up to 9, and a click on one uses it too. Each shows its icon and its key,
and its name and level when the mouse is over it. A skill you don't know yet is greyed out, with
the level it needs, or Learn once you've reached it, and one that's cooling down is shaded, a slice that shrinks clockwise from
the top, with the seconds left in the middle. The client works that out from the milliseconds the
server sends and the cooldown in the skills file.

The server decides it all, as it does a swing. The client sends a skill's id in `Input` once, as its
key goes down, and the world keeps it until the player is free: a skill asked for during a swing or
another skill starts as that ends. Then it starts if the player knows it and its cooldown is over,
and otherwise it goes. A skill holds the player for its cast time, like a swing,
lands partway through, on the monster the player turned to as it started if it's still in reach,
or on each one in reach, and starts its cooldown as it starts. Cooldowns run down whatever the player does, dead or alive,
and aren't saved: a player who leaves and comes back finds their skills ready.

A player using a skill has the motion `MOTION_SKILL`, and its id, so that everyone's clients can
show it. The skills file names each skill's clip, which the client finds in the characters by name,
and plays once, from the start: Thrust plays Mixamo's Thrust Slash, Kick its Kicking, and Whirlwind
its Standing Melee Attack 360 Low, each about as long as the skill holds the player. Second Wind has no
clip, so the player stands, in a green glow, see-through and drawn after the sea, that grows over
the first quarter of a second. One skill straight after another blends from one clip into the next,
as a change of motion does. Your cooldowns are yours alone, so they go with your experience, after the world: the
milliseconds left for each skill that isn't ready, by its id.

### Learning skills

A trainer, an NPC whose kind has `teaches` in the content, teaches a player a skill it teaches,
once they've reached the skill's level and stand within four metres of it, as close as you'd stand
to talk to someone. `World::Learn` checks each of those, and the skill isn't one the player knows
already, or says why not, in words the server can send the player. The world joins each player
with the starting skills, whatever was saved, so the database doesn't need to know which they are.

Click a trainer, and a window lists the skills it teaches, each with its icon and name, and Known,
the level it's from, or a Learn button. The client asks for a skill in `Input`'s `learn`, with the
trainer's id and the skill's, and the server sends a refusal back as a line in the chat, from
nobody, like the storyteller's. The window stays open, and its button turns into Known once the
server has taught you the skill. The skills a
player knows go to them with their cooldowns, after the world, by id.

## Items

Each player carries a bag of 24 slots, and wears an item in each of six slots: the main hand, the
off hand, the head, the body, the legs and the feet. A new player starts with the kit from the items
file (see [Content](#content)), the sword in hand, a linen shirt, cloth trousers and plain shoes on,
and three health potions in the bag. They get it once: a trip through a gate doesn't give them
another, and neither does emptying the bag, so a player from before the clothes has none.

What you wear adds to you: its damage to each blow you land, with a weapon or a skill, its health to
the most you can have, and its armour comes off each blow a monster lands, down to one. The weapon in
your main hand is the one you swing. The server adds it all up as what you wear changes, not as you
fight. Some items roll modifiers of their own as they're made, such as an Ash Blade's damage, which
stay with that one.

Press I, or choose View, Bag, to open the bag. What you wear is in the character window (C). Each
slot shows its item's icon, in the item's tint, and how many are in it, and over it a tooltip says
what it is, where it's worn, what it adds, what it rolled, what using one does and how many stack.
Drag an item onto another slot, in either window, to move it, stack it, swap it, wear it or take it
off, with Shift for half the stack. Double-click it to drink it, wear it or take it off, and
right-click it to throw it away. A health potion heals forty, one every ten seconds.

Monsters drop items, straight into their killer's bag: each kind's `loot` in the monsters file has
the items it can drop, the chance of each, and how many. A skeleton drops bones more often than
not, and now and then a potion, a leather cap, a tunic or boots, and the skeletons of the Ashlands and the
Crypt the Ash Blade and the buckler, the Crypt's more often. What stacks drops as a stack, and what
doesn't one at a time, each with rolls of its own, so two Ash Blades are seldom the same. A line in
the chat says what the monster dropped, and what didn't fit, if the bag was full, which stays
behind. The world keeps those lines as it steps, and the server sends them after the tick.

The server decides it all, as it does a swing. The client sends what you do with an item in
`Input`'s `item`, once, and the world moves the items, with `server/inventory.h`, or says why not,
in a line in the chat from nobody. A player can do ten things with items at once, then ten a
second. Each change takes the bag to its next revision, so telling whether a bag has changed takes a
number, not the bag: the server sends a player their bag, alone, over TCP, as `World`'s `inventory`,
as they join and when its revision moves, which is now and then. Nobody else gets it: others see
what's in your main hand once a second, as your weapon. An item that's gone from the items file
stays in the bag, where it can be moved but not worn or used, so taking one out of the file by
mistake loses nobody's.

## Quests

Some NPCs give quests, which ask you to kill a number of one kind of monster. In the Meadows, the
guard asks for three skeletons from around the gate, for 150 experience and two health potions. In
the Ashlands, the ash warden asks for five Ash Skeletons, for 400 experience and the Ash Buckler.
The healers, in both lands, ask for four from the Crypt. Each quest is in `content/quests.txtpb`: its name, its giver's
kind of NPC, what they say giving it, what it asks you to kill and how many, and the experience and
prizes it gives (see [Content](#content)). A new quest of the same sort is another entry, and needs
no code.

An NPC with a quest for you has a gold mark over its name: `!` for one you can take, or `?` for one
you've done, to turn in. Click it to open its quests: what it says, what each asks for and gives,
and a button to take it, how many you've killed for it, a button to turn it in once you've killed
them all, or Done. The quests you're on are in the top right corner, each with how many you've
killed for it, with a tick once that's all of them.

- The server decides it all, as it does a skill a trainer teaches. The client sends `Input`'s
  `take` or `turn_in`, once, with the NPC's id and the quest's, and the world checks that the NPC
  is of the quest's giver's kind, in the same land, within talking reach, and that you can take or
  turn it in, or says why not, in a line in the chat from nobody.
- A kill counts for each quest you're on that asks for its kind, up to as many as it asks for, with
  a line in the chat, such as "Bones by the Gate: 2 of 3 Skeletons killed.", and the last one says
  who to go back to. Only the killing blow counts, as with experience and loot.
- Turning a quest in gives its experience, which heals you at a new level, as a kill's does, and its
  prizes into your bag, rolled as loot is. They go into a copy of the bag first, so a bag without
  room for all of them gets none, and the quest waits until there's room. A quest turned in is
  done, and can't be taken again.
- Your quests go to you like your bag: alone, over TCP, as `World`'s `quests`, the ones you're on
  with your kills for each, and the ones you've done, as you join and when they change, which takes
  them to their next revision. Nobody else gets them.
- They're saved with the rest of you (see [Saving players](#saving-players)).

## Accounts and logins

- The login server, `nanommo_login`, listens on TCP port 7778. It creates accounts and checks passwords,
  against the `accounts` table. Passwords are hashed with Argon2id, through libsodium, so the
  database never holds a password itself.
- Once you're logged in, it puts a token in Redis, which works once, within 30 seconds, and sends
  it to the client.
- The client takes the token to the world you picked, a game server, `nanommo_server`. The game
  server takes the token out of Redis, and loads your player in that world: your name from the
  `accounts` table, and where you are and your experience from Redis, or from the `players` table if
  Redis doesn't have you.

The password and the token go over TLS, so nobody between the client and the servers can read
them (see [TLS](#tls)).

The servers find MySQL, Redis and Consul on `127.0.0.1`, or wherever `NANOMMO_MYSQL_HOST`,
`NANOMMO_REDIS_HOST` and `NANOMMO_CONSUL_HOST` point. The tests that need them skip themselves
unless those are set:

```bash
NANOMMO_MYSQL_HOST=127.0.0.1 NANOMMO_REDIS_HOST=127.0.0.1 NANOMMO_CONSUL_HOST=127.0.0.1 \
  ./build/bin/nanommo_server_tests
# Or all in Docker, next to MySQL and Redis.
docker compose run --rm tests
```

The world server's test starts a server inside the test, on a port the system picks, and has
players join it over TLS, with the protocol tests' certificates: they see each other until one of
them leaves, and an account can't play twice at once. The server and the players run on the test's
thread, one handler at a time, until what the test waits for happens, or for five seconds at most.

MySQL keeps its data in the `mysql` volume, and Redis in the `redis` volume, so they survive `docker
compose down`. Flyway makes the tables, and changes them, before anything else uses MySQL (see
[Migrations](#migrations)). `docker compose down -v` deletes the volumes, and with them all the
accounts and players.

Or with only Docker installed:

```bash
# Build and run the tests on Ubuntu.
docker build --target test .
# Check the code style.
docker build --target lint .
# The server image, if the tests pass.
docker build --target server -t nanommo-server .
# The login server's image.
docker build --target login -t nanommo-login .
# The saver's image.
docker build --target saver -t nanommo-saver .
# API docs in site/.
docker build --target docs --output site .
```

In Docker, the client's tests open their window on a virtual display from Xvfb, and render with
llvmpipe, Mesa's OpenGL driver that runs on the CPU. So they don't need a screen or a GPU.

## TLS

Everything the clients and the servers send over TCP goes through TLS 1.3, with OpenSSL, through
Asio's `ssl::stream`: logins on port 7778, and the game's connection on port 7777, with the token,
the input and, over TCP, the world. Both ends are ours, so they speak TLS 1.3 and nothing older.

The certificates come from a certificate authority of our own. `tls/certificates.sh` makes its key,
`tls/ca.key`, and its certificate, `tls/ca.crt`. Then a key for the servers, `tls/server.key`, and a
certificate the authority signs for it, `tls/server.crt`, for `localhost`, `127.0.0.1`, and `login` and
`server`, their names in Docker Compose. The keys are ECDSA, on the P-256 curve. Names after the folder
go on the certificate too, such as a server's domain, or its address, which goes on as an IP name:

```bash
tls/certificates.sh tls play.example.com 203.0.113.7
```

It leaves certificates that are there already alone, so delete `tls/*.crt` and `tls/*.key` to make new
ones. Git ignores them, so each copy of the repo has an authority of its own.

- The servers load their certificate and key from `NANOMMO_TLS_CERTIFICATE` and `NANOMMO_TLS_KEY`
  (default: `tls/server.crt` and `tls/server.key`).
- The client and the bots trust the certificates the authority in `NANOMMO_TLS_CA` signed (default:
  `tls/ca.crt`), and no others, and check that the server's name is on the certificate.
- In Docker Compose, the `certificates` service makes them, the first time, before the servers start,
  and the servers and the bots get `tls/` read-only.

Only the authority's key can sign certificates the clients trust, so on a real server, keep it
somewhere else: the servers need `server.crt` and `server.key`, and the players' clients `ca.crt`.

The world over UDP isn't encrypted: it holds what each player sees in the game anyway. Its first
datagram, the hello, carries the login token, which the game server took out of Redis by then, so it
can't log anyone in. Someone who can see your traffic could use it to send your world updates
elsewhere, which someone in that position could stop anyway, by dropping them.

To see the handshake and the certificate, with the login server running:

```bash
openssl s_client -connect localhost:7778 -CAfile tls/ca.crt < /dev/null
```

## Saving players

After each tick, the game server saves the players whose position, experience or health changed, into
Redis: a hash for each player in each world, `player:<world>:<account id>`, with `world`, `account`,
`x`, `z`, `experience`, `health`, `land`, `weapon`, what's in their main hand, and `skills`, the ids
of the skills they know with commas between them, and its key in a set, `players:unsaved` (see
[Worlds](#worlds)). A player whose bag's revision has moved gets `items` too, the bag's `Inventory`
message in protobuf's bytes, and their key goes into `items:unsaved` as well. Their quests go in
`quests`, a `Quests` message in protobuf's bytes, the same the client gets. It sends all of it in
one round trip, as a pipeline, so saving takes a few
milliseconds a tick with hundreds of players, and the server keeps up with as many players as it did
without saving. Writing each change to MySQL instead, one statement at a time, takes the tick past
its 50 ms at around 50 players: each save is two round trips, to prepare the statement and to run
it, and a commit that waits for the disk.

`nanommo_saver` takes the unsaved players out of the set every five seconds, and writes them to the
`players` table, 500 to an `INSERT`, and their skills to the `player_skills` table, from V6, a row
for each skill a player knows in a world. A learned skill stays learned, so the saver inserts each
of a player's skills each time, and `INSERT IGNORE` skips the rows MySQL has already. A player's
skills go with their row in `players`: deleting the account deletes both. If MySQL fails, they go
back into the set, for the next try. A
player whose account was deleted, or whose hash is gone, is skipped, so that one player can't hold
back the others. It saves what's left when it stops.

The saver writes a player's bag again only when it's changed, and not each time they take a step:
it takes each player's mark off `items:unsaved` before it reads their hash, so that a bag that
changes in between is marked again, and writes the bags it took off. The `player_items` table, from
V7, has a row for each stack in a bag and each item worn: where it is, `bag 0` to `bag 23` or the
slot, such as `main hand`, the item, how many, and its `state`, what that one has of its own, such
as its rolls, as protobuf's JSON for a stack. A field a later server adds to a stack goes into the
state with no migration. A bag's rows go, and its new rows go in, which has to happen all or not at
all, or a failure between the two would empty it, so each batch is a transaction, with the players'
rows, which the bags' rows need first. `players.items_revision`, from V8, keeps the bag's revision,
so that a player who emptied their bag isn't taken for a new one, and given the kit again.
`players.quests`, from V9, keeps their quests, as protobuf's JSON for the `Quests` message: they're a
few numbers, read and written whole with the player, and never looked up by quest, so they go with
the player's row, with no table of their own. A hash with no quests, from before them or of a player
on none, leaves the ones MySQL has. So the `players` table, and the website's
rankings, are at most about five seconds behind the game.

Redis appends each change to a file, its append-only file, and writes that to disk once a second, so
a crash loses at most about a second. If the game server crashes, Redis still has its players as of
the last tick. When a player joins, the game server reads them from Redis first, as it's newer than
MySQL, and from the `players` table if Redis doesn't have them, and their bag from Redis, or from
`player_items` when the hash has none.

```bash
docker compose exec redis redis-cli HGETALL player:Ember:1
docker compose exec redis redis-cli SCARD players:unsaved
docker compose exec mysql mysql -unanommo -pnanommo nanommo -e 'SELECT * FROM player_items'
docker compose logs saver
```

## Migrations

The tables are made and changed by migrations, SQL files in `db/` that [Flyway](https://github.com/flyway/flyway)
runs from its official Docker image. Docker Compose runs it as the `flyway` service, and the login
server, the game server, the saver, the website and the tests wait until it has finished. If it
fails, they don't start.

- Each file is named `V<version>__<description>.sql`, with two underscores, such as
  `V2__players_health.sql`. Flyway runs the versions a database hasn't run yet, in order, and records
  each one in the `flyway_schema_history` table, with a checksum of its file. A file whose name it
  can't read stops it, instead of being skipped.
- Don't change a migration once it has run anywhere. Flyway compares the checksums and refuses to go
  on, as a database that ran the old file wouldn't get the change. A change goes in a new file, with
  the next version.
- Write each change so that the servers from before it still work, as they keep running until the
  new ones replace them: add a column that can be `NULL` or has a default, and drop or rename
  things in a later version, once no code uses them. `ALGORITHM=INSTANT` makes MySQL refuse a change
  it can't make by changing the table's definition alone, instead of quietly rebuilding the table,
  which takes a long time on a big one.
- On a server with real players, back the database up before running new migrations.

```bash
docker compose run --rm flyway info      # which versions have run
docker compose run --rm flyway migrate   # run the new ones
```

A MySQL volume from before the migrations has the tables but no history, and Flyway won't touch it.
`docker compose down -v` starts again from an empty one, without the accounts in it. On a database
you want to keep, Flyway's `baseline` records version 1 without running it.

Flyway 13.10 warns that it has been verified with MySQL up to 9.4, and we run 9.7. Our migrations are
plain SQL, and CI runs them on a fresh database on each push. Its URL turns TLS on, as MySQL's default
authentication, `caching_sha2_password`, takes a password over an encrypted connection, or with
MySQL's RSA key, and it trusts the certificate MySQL makes for itself, which is fine on Compose's own
network.

## The website

`web/` is the game's website, a [Laravel](https://laravel.com) app. It ranks the players by
experience, and you can create an account on it, log in and log out. It reads and writes the same
MySQL tables as the servers, so an account made on the website logs into the game, and the other way
round. It hashes passwords with Argon2id, with the login server's settings, in
`web/config/hashing.php`, so each reads the other's hashes. It keeps its sessions in Redis.

It runs in Docker, on [FrankenPHP](https://frankenphp.dev), a web server with PHP built in:

```bash
docker compose up -d --build web
```

Then open http://localhost:8080. [Pico CSS](https://picocss.com), from a CDN, styles the plain HTML,
so there's no CSS of our own, and nothing to build with npm.

- The rankings are the hundred players with the most experience. An account that has never played
  isn't on them. The saver brings them up to date every five seconds.
- Each address gets ten tries a minute at creating an account or logging in, since each try hashes
  a password, which takes 64 MiB and tens of milliseconds.
- The tables come from the migrations in `db/`, which Flyway runs (see [Migrations](#migrations)).
  The website has no migrations of its own, so the tables are described in one place.
- `APP_KEY` in `compose.yaml` encrypts the website's cookies. Like the MySQL password next to it,
  it's for your own machine. On a server, `deploy.sh` makes a new one (see [Deployment](#deployment)).
  To make one yourself, run `docker compose run --rm --no-deps web php artisan key:generate --show`,
  and keep it out of git.

The website's tests, and Pint, Laravel's formatter, which fails on code it would change:

```bash
docker build --target test web
```

## Monitoring and load

Each world serves its metrics for Prometheus at http://localhost:9464/metrics: the players online in each land,
how long each tick takes, how long saving the players takes in each tick, the bytes it sends, the
messages it receives, the lines of chat, said or refused, and the stories asked for, told, refused
or failed, with how long each one took. The bots serve theirs on
port 9465: how old the world on a player's screen would be, by transport.

`compose.yaml` starts the servers, MySQL and Redis together with Prometheus, which finds the worlds
in Consul and scrapes each of them every two seconds, with its name as the `world` label, and
Grafana, with a dashboard of those metrics, world by world. The dashboard and Grafana's data source are files in
`monitoring/`, so they're set up the same way on any machine.

```bash
docker compose up -d --build
```

- Grafana: http://localhost:3000, which opens on the nanoMMO server dashboard.
- Prometheus: http://localhost:9090.
- Pyroscope: http://localhost:4040 (see [Benchmarking and profiling](#benchmarking-and-profiling)).

`nanommo_bot` connects a swarm of bots that play like players. Each one runs to a random place on the
island, within 170 metres of the middle, or on one trip in five into its land's gate, and wanders around there like a player at the keys, running a
third of the time, for 5 to 15 directions of half a second to 3 seconds each, before it sets off
again. When a bot gets where it was going, it says something, one time in four, from a few
lines. Once it has seen its land's storyteller, one trip in eight goes there instead, where it asks
for a story about three words from the game. While it wanders around somewhere, one direction in
four uses one of the skills it knows, from its world, so that the bots' cooldowns go out with their
worlds, as players' do. A bot drinks a potion once it's down to half its health, one every ten
seconds, and wears what it finds in a slot it has free, so that the servers save and send bags as
they would players'. Quests come first: between trips, a bot looks at what's around it, put
together from the world's parts as the client does, and turns in a quest it's done if its giver is
there, or goes after the nearest monster a quest it's on asks for, or takes a quest it hasn't done
from an NPC there who gives it. It runs there, and swings, or takes or turns in the quest, so that
the servers count kills and save quests as they would players'. A bot that stops
getting closer, at the sea or a lake, gives up on its trip. Each one logs in as bot1, bot2 and so on,
and creates its account the first time. It takes the login server's
host, the game server's host, how many bots, how many seconds to spread their arrival over, the
number of its first bot, so that several swarms can play on one server, and the transport for the
worlds, `udp` or `tcp`, UDP by default:

```bash
./build/bin/nanommo_bot localhost localhost 100 10
./build/bin/nanommo_bot localhost localhost 100 10 101 tcp
```

About sixty times a second, like a client's frames, the bots look at how old their newest world is,
from the time the server puts in each world, and Prometheus finds them through Docker's DNS, under the
name `bots`. The bots and the server have to share a clock for that, as they do on one machine.

Or in Docker, next to the server: 300 bots, arriving over two minutes, unless `BOTS` and `RAMP` say
otherwise.

```bash
docker compose up -d bots
BOTS=1000 RAMP=60 docker compose up -d bots
```

A few hundred bots need more open files than macOS allows a shell by default, so on the Mac, run
`ulimit -n 4096` first. Stop everything with `docker compose --profile load down`.

## Alerts

Prometheus checks the rules in `monitoring/alerts.yml` every five seconds, and its alerts page,
http://localhost:9090/alerts, shows each one as inactive, pending or firing:

- `WorldFallingBehind`, when a world has run fewer than 19 ticks a second, out of its 20, for a
  minute. A world that keeps up runs a little either side of 20.
- `WorldDown`, when a world that was one of Prometheus's targets in the last day hasn't been up for a
  minute. A world that stops leaves Consul, and Prometheus stops scraping it, so `up == 0` would miss
  it. A world whose tick is stuck stops its heartbeat too, so Consul drops it, and `WorldDown` fires.

Prometheus sends the alerts to Alertmanager, at http://localhost:9093, which emails each one when it
fires and when it resolves. The mail goes to Mailpit, a mail server for development, which keeps it,
and shows it at http://localhost:8025 and through its API:

```bash
curl -s localhost:8025/api/v1/messages | jq -r '.messages[].Subject'
curl -s localhost:8025/api/v1/message/latest | jq -r .Text
```

To get the mail somewhere else, `smtp_smarthost` in `monitoring/alertmanager.yml` points at a real mail
server, with `smtp_auth_username` and `smtp_auth_password_file`. The rules have tests, in
`monitoring/alerts_test.yml`, which CI runs too:

```bash
docker compose run --rm alerts
```

## Logs

Each server writes what it does to its output: who joins and leaves, what they say, what they
learn, and what goes wrong. `docker compose logs` shows that a service at a time, and loses it with
the container. [Loki](https://github.com/grafana/loki), Grafana's log store, keeps the logs of all
the game's containers, and Grafana queries them next to the metrics:

- Loki runs in a single process (`monitoring/loki.yaml`), with the logs on a volume of its own,
  `loki`, and keeps them for a week. It indexes each line by its labels, and not by what it says, so
  that keeping logs costs little, and a query picks the lines by their labels first, then searches
  what they say.
- [Alloy](https://github.com/grafana/alloy), Grafana's agent, finds the game's containers through
  Docker's socket, which Compose mounts into it read-only: the ones in the Compose project
  `nanommo`, and looks for new ones each five seconds. It reads what each one writes, as it comes,
  and sends it to Loki
  (`monitoring/config.alloy`). Each container's lines get its service's name as the `service` label,
  such as `server` or `saver`, and each line the stream it was written to as the `stream` label,
  `stdout` or `stderr`, which Docker keeps apart. Our servers write their errors to `stderr` and the
  rest to `stdout`, so `stream="stderr"` picks their errors. Other images use `stderr` for
  everything, such as MySQL's.
- Grafana has Loki as a second data source, so Explore, at http://localhost:3000/explore, searches
  the logs with LogQL, Loki's query language:

```
{service="server"}                       # the world server's lines
{service="server"} |= "joined"           # the ones that say joined
{service=~"server|login|saver", stream="stderr"}   # what went wrong
```

The server dashboard has two panels from Loki at the bottom, under the metrics: errors a minute,
the servers' `stderr` lines counted a minute at a time by service with LogQL's `count_over_time`,
and what the servers say, their lines as they come, newest first. A spike on a chart and the lines
from that minute are on one page. Grafana marks each line with its `level` label, which Alloy gives
our servers' lines from their stream, `error` from `stderr`, and `info` from `stdout`.

Loki checks rules on the logs too, like Prometheus checks its rules on the metrics, every 15
seconds, from `monitoring/loki-rules.yml`, and sends the alerts to the same Alertmanager, which
emails them (see [Alerts](#alerts)):

- `SaverFailing`, when the saver has written errors for a minute, such as while MySQL is down. The
  saver keeps going, and the players it couldn't save stay unsaved in Redis until it can, so the
  game doesn't notice, and nothing in the metrics shows it either.

Loki has no tenants of its own here, so its rules are in its tenant's folder, `fake`, the name Loki
gives the tenant when `auth_enabled` is off.

CI checks both configs, as a typo in either would leave us without logs, and Loki and Alloy refuse
a field they don't know:

```bash
docker compose run --rm loki-config      # loki -verify-config
docker compose run --rm alloy-config     # alloy validate
```

Neither looks at the rules' LogQL. A rule Loki can't parse shows up in its log, as `unable to list
rules`, and the rest of its group doesn't load: `docker compose logs loki | grep rules`.

In production, Loki listens on the server's localhost, like Prometheus, and Alloy has no port.

## Benchmarking and profiling

`bench/load.sh` measures how the server copes as players arrive. With the stack from `compose.yaml`
running, it adds bots in steps, 250 at a time by default, and after each step it asks Prometheus about
the last 25 seconds: the players online, the ticks the server managed each second, the tick's p50 and
p99, the megabytes it sent each second, and the p99 of saving the players. It needs `curl` and `jq`.

```bash
docker compose up -d --build
mkdir -p bench/results
bench/load.sh 250 10 > bench/results/before.json
```

The server keeps up while it manages 20 ticks a second. Once a tick takes longer than its 50
milliseconds, the next one starts late, and the world runs slower than real time. The numbers depend
on the machine, so compare runs on one machine, with nothing else busy.

To see where the time goes, `compose.profile.yaml` runs the server from the `profile` image instead:
the server built with Tracy's client, next to perf and Brendan Gregg's flame graph scripts. perf
needs two capabilities in the container: `PERFMON` to sample the server and the kernel, and `SYSLOG`
to read the kernel's symbols. Everything is built with frame pointers, so perf can walk the stacks.

```bash
export COMPOSE_FILE=compose.yaml:compose.profile.yaml
docker compose up -d --build server
BOTS=1250 RAMP=60 docker compose up -d bots
docker compose exec server profile.sh before 10
```

`COMPOSE_FILE` makes each compose command in that shell read both files, the override after the
base. Without it, `docker compose up -d bots` would start the server from `compose.yaml` again.

That samples the server for ten seconds and writes `profile/before.svg`, a flame graph to open in a
browser, along with the collapsed stacks it's drawn from.

[Pyroscope](https://github.com/grafana/pyroscope), Grafana's store for profiles, keeps the servers'
profiles all the time, the way Prometheus keeps their metrics, so there's no image to swap and no
moment to catch. Alloy samples the processes of our servers' containers from the kernel, with eBPF,
and sends Pyroscope what was running, as stacks, each fifteen seconds, with C++'s names demangled
(`monitoring/config.alloy`). The servers' code doesn't change. Loading the program into the kernel,
and reading the other containers' processes, takes privileges, so Alloy runs privileged, in the
host's process namespace; on a Mac, the kernel is Docker's Linux VM's. Grafana has Pyroscope as a
data source, and the server dashboard ends on a flame graph of where the worlds spend their CPU over
its time range, under their metrics and logs. Pyroscope's own page is at http://localhost:4040.

When the bots run on the same machine as the server, part of what the flame graph shows is the bots'
cost: sending a datagram over Docker's network, the kernel delivers it to the bot's socket in the
server's own system call, under `udp_sendmsg`. Players on other machines cost the server only the
sending.

[Tracy](https://github.com/wolfpld/tracy) shows each tick as a frame, with the time spent in the
step, the broadcast, and each client's serialize and send. Its client is in the profile image only,
and records nothing until the profiler connects. The profiler has to be the release the server is
built with, 0.14.1: on a Mac, `brew install tracy`, then connect to `localhost`, port 8086.

The broadcast used to serialize the whole world once for each client, so its work grew with the square
of the players. Now it serializes each player and monster once a tick, and the sessions write those
bytes, followed by each client's own id. Most of the tick goes on the kernel sending the bytes, packet
by packet, which is why each player gets what's near them, and no more (see below).

## Packet loss: TCP and UDP

The server sends each client the world over TCP, or over UDP once the client asks for it. To see what
a bad network does to each, `compose.yaml` has a `netem` service. It runs `bench/netem.sh` in the
server's network namespace, with the `NET_ADMIN` capability, and uses Linux's netem to delay the
server's packets and drop a share of them: a delay in milliseconds, then a loss in percent.

```bash
docker compose up -d --build
docker compose run --rm netem 50 5
docker compose run --rm netem 0 0
```

It turns off segmentation offload on the server's interface first. Otherwise the kernel hands netem
up to 64 KB at a time, to be cut into packets later, and netem drops all of those packets together.
It also lets netem hold 100,000 packets, as its default of 1,000 drops packets of its own once a busy
server has more than that waiting.

`bench/loss.sh` starts 200 bots over the transport you give it, then steps the loss from none to ten
percent, with 50 ms of delay, and after a minute at each step asks Prometheus for the p50 and p99 of
the bots' world age. Like `load.sh`, it needs `curl` and `jq`.

```bash
mkdir -p bench/results
bench/loss.sh tcp > bench/results/tcp.json
bench/loss.sh udp > bench/results/udp.json
```

Without loss, the two are level: a world is up to a tick old, plus the delay. Over TCP, the p99 climbs
with each step of loss, as the worlds wait for each lost packet to be sent again, and at ten percent
TCP can't keep up with 200 clients at all. Over UDP, the p99 stays within a tick or two of no loss. So
the client and the bots take their worlds over UDP by default.

How the world gets there over UDP:

- The client logs in and says hello over TCP, as before. Its input keeps going over TCP, and so does
  anything that has to arrive.
- It then sends its token again, in a UDP datagram, to port 7777, four times a second until worlds
  come back that way. The server sends that player's worlds to the address the datagram came from,
  which home routers let through, as the client spoke first. Until then, or if UDP can't get through,
  the worlds keep coming over TCP.
- The players and the monsters go out in parts of up to 1,200 bytes, so that each fits in a packet and
  IP doesn't split it into fragments, which would all be lost with any one of them. A lost part leaves
  its players and monsters where they were for a tick. `protocol/parts.h` splits the world and
  puts it back together. Each part ends with what's the player's own, their id, experience, seed,
  land, cooldowns and the skills they know, so the parts leave room for that, for the nine skills
  a hotbar holds.

## What each player gets

Each tick, the server sends each player the players and monsters near them, which is called interest
management. Sending everyone the whole world grows with the square of the players, and most of it
would be hidden in the fog anyway.

- `server/interest.h` sorts the players and monsters into cells of 32 by 32 metres each tick, and
  the NPCs once, as they stand still. A player gets their own cell and the two cells each way around
  it, so everyone within 64 metres, and some further out. A player who comes into view does that
  past the nameplates, which show within 50 metres, and partly in the fog. `kInterestCells` sets how
  far that is.
- The players in a cell get the same world, so the server puts it together once a cell. Each player
  and monster is serialized once a tick, into a piece, and a cell's world is its pieces, one after
  another, which protobuf reads as one message (`protocol/parts.h`). Over UDP, the pieces are packed
  into the parts.
- Names, levels and the most health a unit can have go out once a second, with every twentieth world
  (`kFullEvery`), and the client keeps the last ones it had in between. Protobuf leaves out a field
  that's empty or zero, and no name or level is.

With bots that spread over the island, `bench/load.sh 100 12` on an Apple M4 Pro, with the bots on the
same machine as the server:

| Each player gets | 20 ticks a second up to | At 1,200 players | Sent to each player, with 500 |
|---|---|---|---|
| The whole world | 800 players | 10.4 ticks a second | 377 KB/s |
| Within 96 m | 1,000 players | 15.5 ticks a second | 228 KB/s |
| Within 64 m | 1,200 players | 20, ticks of 36 ms (p50) | 135 KB/s |
| Within 64 m, names and levels once a second | 1,200 players | 20, ticks of 26 ms (p50) | 94 KB/s |

On a VPS with 2 vCPUs, with the bots on it too, the game server now keeps its 20 ticks a second up to
about 700 players, where it fell behind past 400.

## Worlds

A world is a game server, `nanommo_server`, with players, monsters and lands of its own. Each one
registers with [Consul](https://developer.hashicorp.com/consul), HashiCorp's service registry, and
the login server asks Consul for the worlds that are up, so a new world needs no change to the
client, the login server or Prometheus. Compose runs two: Ember on port 7777, and Frost on 7787,
on an island from another seed. Consul's UI is at http://localhost:8500.

A world takes its port as its first argument (7777 by default), and the rest from settings:

| Setting | Default | What it is |
|---|---|---|
| `NANOMMO_WORLD` | `Ember` | Its name, up to 16 letters and digits |
| `NANOMMO_WORLD_SEED` | `1` | The seed its lands' islands grow from |
| `NANOMMO_PUBLIC_HOST` | `localhost` | Where players reach it, on its port; Compose takes it from your shell |
| `NANOMMO_METRICS_PORT` | `9464` | Where Prometheus reaches its metrics |
| `NANOMMO_CONSUL_HOST` | `127.0.0.1` | Where Consul is, on port 8500 |
| `NANOMMO_CONTENT` | `content` | The content folder (see [Content](#content)); the image has its own |

- A world registers with its name, its address, the public host and port, the metrics port, how
  many players it has and when it started, and a health check that turns critical ten seconds
  after its last pass.
- Every two seconds, the tick loop sends a heartbeat, and a thread of its own takes it to Consul, so
  a slow Consul can't hold up a tick, and a heartbeat that finds the last one still waiting skips.
  Each heartbeat passes the world's check, so a world that stops ticking drops out of the list, and
  comes back when it ticks again. A world registers again when its players change, as registering
  again keeps the check's status, and after an error, as Consul forgets its worlds when it restarts.
  Consul forgets a world that stays critical for a minute.
- A world registers the address the kernel sends to Consul from, which is the address Consul and
  Prometheus reach it at, inside Compose's network. Players reach it at its public host instead,
  which in Docker is the machine the ports are published on.
- When it stops, a world deregisters, and leaves the list at once.
- A world's name is its ID in the `players` table, so two worlds with one name share their players.
  In Consul, each instance of a world registers under its name and port, such as `Ember-7777`, as a
  world can run as two instances for a moment, on two ports. The login server lists each world
  once, at its newest instance, and Prometheus labels each instance with its world, from the
  `world` in its registration.
- To play from another machine, start the worlds with the address of this one:
  `NANOMMO_PUBLIC_HOST=192.168.1.20 docker compose up -d`.
- Consul runs in development mode: one server, which keeps everything in memory. Each world
  registers again within two seconds, so a restart loses nothing for long. In production, Consul
  would run as three or five servers, so that one can fail.

A third world, with nothing changed in a file:

```bash
docker compose run -d --name gale -p 7797:7797 -p 7797:7797/udp \
  -e NANOMMO_WORLD=Gale -e NANOMMO_WORLD_SEED=3 server 7797
docker stop gale && docker rm gale
```

Each world keeps its own players: the `players` table has a row per account and world, and the
players in Redis are `player:<world>:<account>`. The players from before there were worlds stay in
Ember: the migration gives the table's `world` column Ember as its default, which also keeps the
servers from before saving while a deploy migrates, and the saver writes a player that was unsaved
before worlds to Ember. The rankings on the website show each player's world. The bots connect to
port 7777 of the host they're given, so they play in Ember (`server` in Compose).

## Lands

Each world has three lands, each an island of its own: the Meadows, where players start, the
Ashlands, where ash and charred trees stand around lava, under a red haze, and the Crypt, a dungeon
of grey stone around a green ooze. `terrain/lands.h` lists them for the server, the client and the
bots, in one order, as the server tells a client which land it's in by its place in the list, and
lists the gates between them:

| Land | Seed | Gates |
|---|---|---|
| Meadows | the world's seed | 10 metres north of the middle, to the Ashlands; 12 metres east, to the Crypt |
| Ashlands | the world's seed + 1000 | 10 metres north of the middle, to the Meadows |
| Crypt | the world's seed + 2000 | 10 metres north of the middle, to the Meadows |

The world server runs its lands as copies, in its one tick loop, each a `World` with its own players
and monsters (`server/copies.h`). Each open land has one copy, which everyone in the world shares. A
dungeon has one for each player who goes in, made as they go through its gate, with its own
monsters, so that nobody else is in it:

- A dungeon's copy that has nobody in it stops: its monsters stand where they are, and don't come
  back to life. It waits five minutes for its player, so that someone who steps out finds it as they
  left it, and then it goes.
- The players in a dungeon are saved outside it, three metres out of its gate in the land it leads
  to, as its copy may be gone by the time they're back. After a deploy it is, as it ran in the
  instance before: a player in a dungeon comes out of it, in the next instance, where they went in.
- A copy that has nobody in it gets nothing broadcast.
- The `nanommo_copies` metric counts each land's copies.

- Players wade into the Meadows' sea up to their knees, and not into the Ashlands' lava: each land
  says how deep its sea can be waded, and its `Terrain` takes that.
- A gate is a ring of standing stones around a slab in the colour of the sky of the land it leads to.
  A player who walks within a metre of its middle goes to that land, and comes out three metres past
  the middle of its twin there, the gate that leads back, the way they were facing, so that they walk
  on out of it. Each gate has a twin, and each dungeon a gate, which a `static_assert` checks.
- The server sends each client its land and its land's seed, and the client builds that land in its
  look, from `client/atmosphere.h`, whenever the seed changes. That takes a moment, as it makes the
  island and its chunks again.
- The `players` table and the players' hashes in Redis keep the land a player is in, by its name, so
  they come back where they left. The migration gives the column the Meadows as its default, which is
  where the players from before were, and keeps the servers from before saving while a deploy
  migrates.
- The players-online metric has a `land` label, and the dashboard shows each world's lands apart.
- The bots go into one of their land's gates on one trip in five, so they spread over the lands, and
  each that goes into the Crypt gets a copy of its own.

Running a world's copies in one process makes a trip through a gate a move from one `World` to
another, within a tick. With a server for each land, each land could have a machine of its own, and
a trip would hand the player over from one server to the other, with a save and a token, like a
login.

## Chat

Enter opens a field at the bottom of the chat window, in the bottom left corner, and Enter again says
what you typed to everyone in your world, in all its lands, under your name. Escape leaves the field.
While you type, the game's keys do nothing, so a line doesn't walk you, turn the camera or swing
your sword.

- A line goes to the server with your input, as `say`. The server sends the lines said in each
  tick to each player when the tick ends, in one message, which has nothing but chat in it.
- Chat goes over TCP, even to a client that gets the world over UDP, as each line has to arrive,
  while a world that's lost is replaced by the next one.
- The server cleans each line (`protocol/chat.h`). Control characters, such as a new line, become
  spaces, so nobody can start a line that looks like someone else's, and the spaces at either end
  go. An empty line, or one longer than 200 bytes, goes nowhere. Protobuf refuses a line that
  isn't UTF-8.
- Each player can say five lines at once, and then one every two seconds, from a token bucket that
  fills up again over time (`server/token_bucket.h`). A line over that goes nowhere, and the server
  tells the player that they're talking too fast, in a line with no name, which the chat window
  shows in yellow.
- The client keeps the last hundred lines. The server prints each line said, and counts the lines,
  said and refused, in `nanommo_chat_lines_total`, which the dashboard shows a minute.

Each line goes to everyone in the world, so with a thousand players in a world, the server sends
each line a thousand times. Chat for the players nearby, or for a party or a guild, would send each
line to far fewer.

## NPCs

Each land has a few NPCs near its middle, where players start. In the Meadows, a guard and a
storyteller stand on either side of the road to the gate, and a merchant and a healer face each
other across it. The Ashlands have an ash warden and a storyteller, a merchant and a healer. Each
land has a trainer too, a few steps from where players start. The storytellers tell stories (see
[The storyteller](#the-storyteller)), the trainers teach skills (see
[Learning skills](#learning-skills)), the guard, the ash warden and the healers give quests (see
[Quests](#quests)), and the merchants don't do anything yet. Each one is a person, like the
players, with its name over it in green.

Kinds of NPC, and where each NPC stands, are in `content/npcs.txtpb` (see [Content](#content)).
The server leaves out an NPC whose ground isn't walkable, which another world's seed could do.

An NPC isn't a monster that stands still. It has no health, level or experience, and nothing to
think with, so it's a small `Npc` of its own in each land's `World`, with ids of its own, apart
from the monsters'. NPCs are never saved, as the content puts them back where they were whenever a
server starts.

- NPCs never move, so the server puts them into their interest cells once, when it spawns them, and
  each player gets the NPCs near them, like the players and monsters (see
  [What each player gets](#what-each-player-gets)).
- They never change either, so they go out once a second, with the names and levels, in every
  twentieth world, by kind.
- The client keeps each NPC for three seconds after the world it last came in was sent
  (`kNpcTimeout`, in `protocol/parts.h`), so one lost datagram, or two, doesn't make them blink out
  over UDP. It drops one that's been gone for longer, such as when you walk away from it, and a new
  land starts with none.
- The client draws every NPC as a person, on the players' characters, in what its kind wears, with
  its own skin and hair (see [Characters](#characters)), and picks one with a box around where a
  person stands. Until episode forty-five, each was a ball of its kind's colour and size.

By kind, the Meadows' four NPCs take 74 bytes in each world they go in, half of what their names and
looks took.

An NPC that walks a round, or whose look changes, would have to go out with every world, like a
monster, or in a message of its own whenever it changes.

## Content

The game's content is data, in the `content` folder, which the world servers, the client and the
bots read, so that there's one place to change it: `figures.txtpb`, `items.txtpb`,
`monsters.txtpb`, `npcs.txtpb`, `quests.txtpb` and `skills.txtpb`, in protobuf's text format, each
with its schema next to it, in `figures.proto`, `items.proto`, `monsters.proto`, `npcs.proto`,
`quests.proto` and `skills.proto`. The figures are the players' bodies and hair (see
[Characters](#characters)). There are eleven items: the sword, a health potion, a bone, a leather
cap and tunic, the Ashlands' blade and buckler, the clothes a new player starts in, a linen shirt,
cloth trousers and plain shoes, and leather boots, three kinds of monster, the skeleton, the Ash
Skeleton and the Crypt Skeleton, six kinds of NPC, three quests (see [Quests](#quests)) and four
skills (see [Skills](#skills)).

- An item, or a kind of monster or NPC, has an id and a name, and parts that each side reads what it
  needs from. A weapon's `weapon` and a monster's `fight` are the server's: reach, swing and hit
  times, damage, and for a monster, its sight, its leash and how far it wanders. A `look` is the
  client's: an item's or a monster's model, its length or height, where a weapon is held, an
  item's icon and tint, a monster's tint, and an NPC's skin, hair colour, way to wear its hair,
  weapon and clothes, and the colour of what it wears that's dyed. A monster's level, health,
  experience and loot are the server's too.
- An item is the parts it has, and no type: `stack` says how many go in a slot of a bag, `equip`
  where it's worn and what it adds, as `modifiers` of `STAT_DAMAGE`, `STAT_HEALTH` or
  `STAT_ARMOUR`, `use` what using one does and how often, and `rolls` the modifiers each one gets
  as it's made, from `min` to `max`. A new kind of item that puts parts we have together is
  another entry, and needs no code. The kit, `starting`, is what a new player starts with, and
  `retired` lists the ids of items that are gone, which no item can take again. A skill's `effect` is
  the server's, and its `look` the client's: its icon, its clip and its glow. The client shows a
  skill's level and cooldown too, so those are in both. `starting` names the skills every new
  player knows, and a kind of NPC's `teaches` the skills it teaches, by id.
- Monsters with a fixed home, camps of monsters and NPCs name their kind by id, and their land by
  name. A quest names its giver's kind of NPC, the kind of monster it asks for and its prizes by id.
- Each file's first two lines, `# proto-file:` and `# proto-message:`, name its schema, so that
  editors and tools that know text protos can check it and complete its fields.
- `content/content.h` (`nanommo_content`) reads the folder with protobuf's `TextFormat`, so a field
  the schema doesn't have, or a value of the wrong type, is an error with the file and the line it's
  on. Then it checks what types can't: every item and kind has an id, no two share one, an item's
  parts go together (a weapon is held in the main hand, what's worn has a slot, and what stacks
  isn't worn and doesn't roll), a roll goes from less to more, the kit's items are in the file and
  fit in a slot, no item has a retired id, each spawn and camps names a kind and a land we have, no monster with a
  fixed home has a leash that reaches the middle of its land, where players come back to life,
  each skill has a level and lands before it's over, each starting skill is in the file, and each
  drop has a chance from nothing to one, and goes from fewer to more, each kind of NPC has a skin,
  and each quest has a giver, asks for some kills, and gives some of each of its prizes. Once every
  file is read, `CheckContent` checks what no file can on its own: that each item a monster drops is
  in the items file, each skill a trainer teaches is in the skills file, each NPC's hair is in the
  figures file and its clothes in the items file, and each quest's giver, monster and prizes are in
  theirs. The server, the client and the bots don't start without good content.
- Ids start at one, and an item or a kind keeps its id for good, as they're saved and sent by id.
  Protobuf leaves a zero out, so the client takes a missing weapon or kind as one it already knows.
- Each player's bag holds its items by their ids (see [Items](#items)), saved with the rest of the
  player (see [Saving players](#saving-players)). A player who's never had a bag gets the kit, so
  the database doesn't need to know what's in it. What's in a player's main hand is still saved
  in the `weapon` column of the `players` table from V5, for the servers from before bags.
- Players' weapons, and monsters and NPCs, go to the clients by id, once a second, with the names
  and levels, and the client looks up the name and the look in its content. The fields that carried
  them before, a monster's name and an NPC's name, colour, radius and whether it talks, are reserved
  in `nanommo.proto`, so that no new field takes their numbers. The client draws a monster of a kind
  it doesn't have as a cube.
- The world servers and the bots read the folder from `content`, under where they start, or from
  `NANOMMO_CONTENT`, which their images set to their own copy, in `/usr/local/share/nanommo/content`.
  The build copies the files next to the client, which reads them from there.

Each side reads its own copy, so a client and a world have to agree on what the client reads. Each
world works out its `ClientHash`, a 64-bit FNV-1a of the ids, names and looks, what the bag shows of
each item, which is all of it but its weapon, the skills' levels and cooldowns, what each trainer
teaches, and all of each quest, serialized deterministically, registers it with Consul, and the login server
lists it with the world. The fights and the skills' effects stay out of it, so that changing a
sword's damage or a skeleton's leash needs no new client.
A client lists a world with other content than its own as such, and doesn't log in to it. The servers
and the client print their hash as they start.

A change to the content, such as the sword's reach, is an edit to a file and a restart of the world
servers, and a rebuild of the client when it changes what the client reads. A new item or kind is
another entry, with the next id, as the Ash Skeleton was, and a monster drops it once its kind's
loot names it.

## The storyteller

The storyteller is an NPC that tells you a story about a few words you give it. Click it, in purple
across the road from the guard, or from the ash warden, type a few words, such as
`dragon castle gold`, and press Enter, or Tell me a story. The story comes back in the chat, under
the storyteller's name, to you and nobody else.

The stories come from [mvGPT](https://github.com/nenuadrian/mvGPT), a small language model built
from scratch in a series of its own, which learned to write short children's stories from a few
words, and is on the Hugging Face Hub at its `ep003-sft` tag. `storyteller/` is mvGPT's completions
server, from its `ep014/end` tag: an OpenAI-style `POST /v1/completions` on port 8000, which writes
the requests that wait at the same time together, up to 16 at once (see `storyteller/README.md`).
Compose runs it as the `storyteller` service, and keeps the model it downloads the first time in a
volume:

```bash
docker compose up -d --build storyteller
curl localhost:8000/v1/completions -H 'Content-Type: application/json' \
  -d '{"prompt": "Words: dragon, castle, gold\nStory:", "max_tokens": 100}'
```

- A click on the world makes a ray from the camera through the mouse (raylib's
  `GetScreenToWorldRay`), and the client picks the nearest NPC that talks along it, by a box around
  where a person stands (`GetRayCollisionBox`, in `client/picking.h`). Each NPC says whether it
  talks, from its kind.
- The words go to the server with your input, as `talk`, with the NPC's id. The server checks that
  the NPC is in your land and talks, and that you're within four metres of it, and cleans the words
  like a line of chat (see [Chat](#chat)).
- The prompt is the one mvGPT learned from, with the first three words, in lowercase:
  `Words: dragon, castle, gold\nStory:` (`server/stories.h`). Its vocabulary is the 10,000 commonest
  words in its stories, so it knows dragon, castle and sword, and a word it doesn't know, such as
  warden or healer, reaches it as unknown.
- Each player can ask for a story every ten seconds, from a token bucket of their own. The server
  tells a player who's too far away, or asks too soon, in a line with no name.
- The server asks the storyteller for a hundred tokens at most, from 16 threads of its own
  (`server/storyteller.h`), so that a tick never waits for the model, and the storyteller can write
  16 stories at once. A story takes a second or two. One that takes longer than ten seconds fails,
  and so does one when the storyteller is down, and the NPC says it can't think of a story.
- A story that ran out of tokens stops in the middle of a sentence, so the server ends it at its last
  full stop, and breaks it into lines of chat of at most 200 bytes, after the last sentence that
  fits. Each line is cleaned like a player's, and they go to you over TCP, like chat.
- The server counts the stories in `nanommo_stories_total`, told, refused or failed, and how long
  each took, from asking to telling, in `nanommo_story_duration_seconds`. The dashboard shows the
  stories a minute, and the median and 99th percentile wait.

The world server finds the storyteller at `NANOMMO_STORYTELLER_HOST` (`127.0.0.1` by default), which
Compose sets to `storyteller`. Any server with OpenAI's completions API can take its place, such as
vLLM, as long as its model writes stories from words like these.

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

## Deployment

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
that, the server's Docker keeps ccache's cache mount (see [Checking the code](#checking-the-code)),
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
second up to about 700 players (see What each player gets).

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
