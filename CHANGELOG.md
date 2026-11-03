# Changes

What changed in each version of nanoMMO, the newest first.

A version is MAJOR.MINOR.PATCH, from `project()` in `CMakeLists.txt`. While the major version is
0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes one without
changing what the client and the servers say to each other. The login server lets in a client of
its own major and minor version, and turns any other away.

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
