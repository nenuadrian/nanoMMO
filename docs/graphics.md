# Graphics and sound

## Characters

The players are people, posed by Mixamo's X Bot and Y Bot, with an idle, a walk, a run, a sword
slash and a death, and a clip for each skill that has one: a thrust, a kick and a spin.
Mixamo's terms don't let us share its files, so they're not in the repo:
[assets/mixamo/README.md](../assets/mixamo/README.md) lists what to download from mixamo.com. Then make
the characters:

```bash
docker build --target characters --output assets/characters .
```

The `characters` stage converts the FBX files to glTF with FBX2glTF, and
[assets/tools/merge.mjs](../assets/tools/merge.mjs) puts each character and the clips into one `.glb`
that raylib can load: one skeleton, the walk and the run in place, and a tenth of the triangles. The
slash and the death keep their motion, so a player falls back as it dies. The build copies
`assets/characters` next to the client, which loads them from there. Without them, the players are
cubes, and the client's character tests skip themselves.

The client draws neither bot's mesh. A player's figure is made of parts, in
[content/figures.txtpb](../content/figures.txtpb): limbs with round ends, from one joint of the rig to
another, and low-poly balls at a joint, each in metres, along the figure's own axes as it stands in
the rig's T-pose. A part on the left can be mirrored onto the right. The client builds the parts'
triangles where the bones rest, and binds each corner to its part's bone alone, so the clips pose the
parts as they posed the mesh, rigidly, like a jointed doll. The body's parts are white, which takes
on the player's skin, with dark eyes. The file lists the skins, the hair colours and the ways to wear
the hair, and each player's id picks one of each, the same on every client. An item's look can have
parts too, which a player who wears it has over their figure, in their own colours, or for a dyed
one, such as the Linen Shirt, in the player's colour (see [Items](gameplay.md#items)). Each player in the
world's message says what they wear, so everyone sees each other in it. NPCs are people too, but
their kind in the NPCs file says their skin, hair and clothes, which a player's id doesn't pick, and
which of the two bots they're on (see [NPCs](npcs.md)). With cel shading on, the
outline is drawn with each figure's hull, the same parts with smooth normals, so it stays whole
around the parts, but for those flush with a bigger one, such as the eyes. Nothing casts a shadow,
so each figure has a soft dark patch under it, on the ground.

Each player holds the weapon in their main hand (see [Items](gameplay.md#items)), so far Quaternius's sword, from
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
the repo: [assets/music/README.md](../assets/music/README.md) says where to get it. The build copies
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
