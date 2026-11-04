# Graphics and sound

## Characters

The players are people, built on a skeleton of our own, which moves in ways of our own: an idle, a
walk, a run and a death, and the skills, Strike on Space too (see [Moves and fx](#moves-and-fx)).
All of it is in the repo, as numbers, in [content/rig.txtpb](../content/rig.txtpb), so a clone has
everything it needs to draw them.

The rig is a list of joints, each with the joint it hangs from and where it is, in metres, along
the figure's own axes as it stands with its arms out to the sides: the hips, which carry the rest,
three joints up the spine, the neck and the head, and on each side the collarbone, the shoulder, the
elbow and the wrist, and the hip, the knee, the ankle, the ball of the foot and the tip of the toes.
The lengths are an adult's, as shares of their height. A joint on the left can be mirrored onto the
right. A build moves some joints, and the ones that hang from them, such as the shoulders out
wider, and each player's id picks one of the builds (`client/rig.cc` makes a raylib model of each,
a bone for each joint, resting where it is, unturned).

How a figure stands is a key like a skill's, turning the joints from the arms out to arms down by
the sides, with the sword hand turned so the blade points ahead. The idle and the death are moves
like a skill's, over the stance: the idle breathes, and shifts the weight from one foot to the other,
and loops; the death jerks back, doubles over, sits down hard, rolls onto its back and lies there.
The ground holds the dying body up: whatever a key puts under it is lifted out of it.

The walk and the run are numbers rather than keys: the speed, how long a step with each foot takes,
the share of it each foot is down, how high a foot lifts, how far apart the feet are, how far the
hips drop, bob and sway, how far the body leans, the hips and the chest turn and the arms swing, and
how far a foot rolls off its heel as it lands and onto its toes as it leaves the ground. A foot
that's down goes back under the body exactly as fast as the server moves the body forward, so it
stays where it was put down, then it lifts and swings forward to land again; and the middle of its
time down is where the leg reaches as far forward to land as it does back to push off. The legs
reach the feet by bending at the knee, in the plane of the hip, the ankle and the way the foot
points, as two bones of fixed lengths do, and past most of the leg's length the foot falls a little
short, more so the further it is, so the knee straightens smoothly. The sword hand bends back as its
elbow does, so the blade keeps pointing ahead.

As it loads, the client works out each motion, sixty frames a second, into raylib's own clips, which
it plays and blends as it would any model's.

A player's figure is made of parts, in [content/figures.txtpb](../content/figures.txtpb): limbs with
round ends, from one joint of the rig to another, and low-poly balls at a joint, each in metres,
along the figure's own axes as it stands with its arms out. A part on the left can be mirrored onto
the right. The client builds the parts' triangles where the bones rest, and binds each corner to its
part's bone alone, so the clips pose the parts rigidly, like a jointed doll. The content's checks
refuse a part on a joint the rig doesn't have. The body's parts are white, which takes
on the player's skin, with dark eyes. The file lists the skins, the hair colours and the ways to wear
the hair, and each player's id picks one of each, the same on every client. An item's look can have
parts too, which a player who wears it has over their figure, in their own colours, or for a dyed
one, such as the Linen Shirt, in the player's colour (see [Items](gameplay.md#items)). Each player in the
world's message says what they wear, so everyone sees each other in it. NPCs are people too, but
their kind in the NPCs file says their skin, hair and clothes, which a player's id doesn't pick, and
their kind's id picks their build (see [NPCs](npcs.md)). With cel shading on, the
outline is drawn with each figure's hull, the same parts with smooth normals, so it stays whole
around the parts, but for those flush with a bigger one, such as the eyes. Nothing casts a shadow,
so each figure has a soft dark patch under it, on the ground.

Each player holds the weapon in their main hand (see [Items](gameplay.md#items)), so far Quaternius's sword, from
[Poly Pizza](https://poly.pizza/m/ajOJ2NLz5m), which is public domain and in the repo, as it was
downloaded, in `assets/weapons`. The client scales it to its length in the file, a metre, holds it
where the file says, a sixth of the way up, and, each frame, puts it where the right hand's bone is
in the pose: along the knuckles, with the blade out past the thumb, leaning a little towards the
fingers. glTF colours are linear,
so the client turns the sword's into sRGB, as our shader writes colours out as they are. An item's
tint goes over them, so the Ash Blade is the sword in another colour.

The server decides which way each player faces, and whether it stands, walks or runs. It moves them
at 1.8 and 3.1 metres a second, the speeds of the walk and the run, so that the feet don't slide.
The client blends from one clip into the next over a quarter of a second, and smooths each player's
position between the server's ticks.

## Moves and fx

A skill's move, in its look in the skills file, is a list of keys, each a pose at a time into the
skill. A key turns some of thirteen joints, from the hips and the chest to the knees, each a bone of
the characters (`kJointBones` in `client/moves.h`), by a pitch, a yaw and a roll in degrees, about
the player's own axes: the one out to their left, up, and the one they face along, the way your
fingers curl with your thumb along it. So a positive pitch bows the chest forward and swings a
hanging arm back. A key's `lift` and `ahead` move the whole body, in metres, for a jump or a lunge,
and its ease is how the joints get to it from the key before: `SMOOTH`, `SNAP` for a strike, fast
away and slowly in, or `GATHER` for a wind-up. Before the first key, the move is the clip under it,
the idle, and it holds its last key, which is that again, with no turns, unless a turn is a whole
one, as Whirlwind's two turns round are. The content's checks refuse a move whose keys go back in
time, or which runs on after its skill is over.

The client poses the clips as before, then turns the bones by the move (`client/moves.cc`): the
bones come parent first, and each joint takes the bones below it round with it, about itself, so a
turn of the chest moves the arms and the head. raylib skins on the GPU, from a matrix for each bone,
from where it was bound to where it is now, so those are worked out again. Each turn is about the
axes as the joints above it left them, so an arm swung forward stays forward of a chest that's
turned.

A skill's fx, in its look too, are what it shows besides the move: each a kind, where it is (at the
player's feet, the tip of their blade, or the feet of the monster they picked, without which it's
left out), when it starts and for how long, in milliseconds into the skill, a colour, a size in
metres and a count, and how far `up` it is from there: a cut across a monster's chest is a metre up
from its feet. There are twelve kinds: `TRAIL`, a ribbon where the blade has just been; `GLOW`, a ball of light; `RING`, a wave along
the ground; `SPARKS`, which fly out and fall; `MOTES`, specks that rise and swirl; `SHAKE`, which
shakes the camera of anyone within 15 metres, less out to 40; `SHARDS`, pieces thrown up that fall
back and shrink away; `SPIKES`, which come out of the ground nearest first and sink back; `BOLT`,
lightning from the sky; `WAVE`, a crescent that flies forward; `EDGE`, the blade lit along its
length, a bright core with a glow at its tip; and `SLASH`, cuts that hang in the air, flat to the
camera, so that each reads as the stroke that made it, drawn across from one end in a flash and then
thinning away, slanted by the fx's `angle` and as many as its count, each a little after the one
before and at a slant of its own.

Each is worked out from how long ago the skill started, with numbers that look random but are the
same every frame, for each cast, so an fx looks the same at any frame rate, in every capture. The
client keeps each cast going until its last fx is over, after the player's done with the skill. A
trail poses the blade again at moments just gone, sixteen of them over the last 0.16 seconds, and
joins where it was into a ribbon: see-through at the hilt and brightest at the tip.

The shards and the spikes are solid, a five-sided gem scaled and turned, lit by the world's shader
like the rest of it, and drawn with the figures. The rest is light: drawn last, from both sides,
over what's behind it without hiding it. The
light's colour goes out three times as bright, past white, so it blooms. It's blended over what's
behind it rather than added to it, as light added to the grass's green goes green itself.

A blow that lands holds the player and what it hit still for a moment, the skill's `stop_ms`, half as
long again for each metre it pushes, up to two and a half times as long. The client decides, as the
cast comes to each blow, whether it lands, from where it sees everyone: what it goes for is alive and
within its reach, and half a metre more, as we see everyone a moment late. Then the player's clips
and the monster's run at a twentieth of their speed for the hold, and catch up over one and a half
times as long, so the skill still ends when the server says (`Fx::Hold`). The fx that go with the
body, the trail and the lit edge, are held back with it, and the rest, such as the sparks a blow
throws out, fly through the hold. Each hit the server sends lights the monster up, by the shader's
emission, fading over 0.15 seconds, and rocks it back, tipped about its feet away from whoever hit
it, in 0.03 seconds, and slowly upright again (`client/reactions.h`).

## The loading screen

Entering a world, the client used to wait on the login and the connection with the login screen
frozen, then draw a land from no seed until its world came; going through a gate, it built the new
land in the middle of a frame, and the land appeared all at once. Now a loading screen covers the
world from the moment you enter one, in the sky's colour, with the world's name, and once the world
says which land you're in, the land's (`client/loading.h`).

- Connecting and building a land each hold up the frame they happen in. So each waits until a
  frame of the cover has been drawn: `Loading::covered()` is true only once the frame after
  `Cover()` is on the screen, and the land is built in the next one, behind it.
- Once the land is built, the cover fades over 0.4 seconds, and the fog lifts over a second and a
  half, fast at first and slower as it ends: it starts two metres from the camera and goes out to
  the land's own, so what's nearest comes out of the sky first. The renderer draws the atmosphere
  with the fog as far as it's lifted (`Renderer::set_clear`), and leaves the land's as it is.
- A frame that took long, such as the one that built the land, counts as a thirtieth of a second, so
  the fade and the fog don't jump.
- `Loading` is kept apart from raylib and ImGui, so its tests need no window, and
  `DrawLoadingScreen` draws it on ImGui's foreground list, over the windows, with dark words on a
  light sky and light words on a dark one.

## The map and the minimap

As a land is built, behind the loading screen, the client draws it from above into a texture, a
pixel a metre, north up (`client/map.h`): its ground in the land's colours, as the meshes have
them, lit from the top left by its slopes, its shallow and deep sea, and each tree as a dark spot.
The minimap, in the bottom right, shows the 45 metres around you, and M opens the map of the whole
land, which Escape or M closes. Both show the land's gates, in the colour of where they lead, the
NPCs and their quest marks, the monsters, with a ring around the one you picked, the other players,
where you're walking to, and you, as an arrow the way you face. A click on the map walks you there
(see [Walking by yourself](worlds.md#walking-by-yourself)).

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
  makes, Music, Effects (see [Sound effects](#sound-effects)), and Mute, with the track's credit
  under them. Each slider goes
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

## Sound effects

The steps, the skills and the gates have sounds the client makes from numbers as it
starts, in about a third of a second, so there are no files for them in the repo.

- `client/synth.h` has what they're made of: samples, 44,100 a second, each where the speaker's cone
  is, from -1 to 1; noise, random numbers from a seed, so that a sound made again from its seed comes
  out as it did; a state variable filter, which passes what's below a frequency, around it, or above
  it, and can sweep from one sample to the next; a sine whose pitch can glide; and an envelope, which
  rises and then dies away.
- `client/foley.h` has the sounds. A step is the heel, then the rest of the foot 40 to 70
  milliseconds later, and what's underfoot makes the rest: a soft thud and a brush on grass, a hiss of
  grains on sand, a crunch on snow, a dry crack on the Ashlands' ash, a knock and some grit on rock, a
  knock that echoes on the Crypt's stone, and a splash and bubbles in the sea. `GroundAt` finds the
  ground: the sea, or else the surface the ground's colour comes from (`SurfaceAt` in
  `client/ground.h`), which each land makes a ground of its own (`kLandSounds`).
- A skill's sound is made from its fx (see [Moves and fx](#moves-and-fx)): each kind has a sound,
  played at the fx's `from_ms`, so a new skill in the skills file sounds without a sound of its own.
  Fx at the target sound with a monster picked, as they're drawn with one. A trail is a whoosh,
  sparks ping like metal, with a thud where they fly off a monster, a glow is a chord, a ring thumps
  and rushes out, motes are notes from a scale with no notes that clash, a shake booms, shards and
  spikes crack and knock, a bolt buzzes and thunders, a wave roars, a lit blade sings, a high note
  that wavers, and each cut hisses, snaps and rings. The fx's colour sets the
  pitch, from 110 hertz for red to 440 for blue, so frost rings higher than fire, and ice shards ring
  like glass where earth knocks.
- Going through a gate, a sound rises two octaves as the loading screen covers the world. Coming out,
  and entering the world, the land's chord is struck like a glockenspiel's bars: C major in the
  Meadows, A minor in the Ashlands, and a diminished chord in the Crypt.
- `client/soundboard.h` makes eight takes of each step and three of each skill, and plays a take at a time on
  raylib's sound aliases, which share the take's samples, so that a sound can play again before it's
  over. A take doesn't follow itself, and each plays a little higher or lower, and a little quieter
  or not (`Variety`, in `client/soundscape.h`), so that a hundred steps don't sound like a machine.
- A step comes when the walk or the run puts a foot down. As a character loads, the client finds
  when each foot's bone comes down to near its lowest in each clip: our walk and run put the left
  foot down as each cycle starts, and the right halfway. The skeleton has no foot bones, so it steps
  at the start and halfway.
  `Footsteps` follows how far each figure is through its clip, and steps when a foot's moment comes
  round.
- Each sound is heard from where you stand: in full within four metres, then falling as one over the
  distance, and gone at forty, and from the side it's on, as the camera faces.
- View, Settings has Effects, next to Music, for the steps, the skills and the gates.
- `./build/bin/nanommo_sounds` plays each sound outside the game: up and down pick one, and Space
  plays a new take of it and draws it.

## The sky

The sky over each land is drawn first, behind everything else, as a ball around the camera
(`client/sky.h`): its colour goes from the land's `sky` at the horizon, which the fog fades into, to
its `zenith` overhead, and the sun is a disc brighter than white, so the bloom spreads it, with a glow
around it. Each land's `Atmosphere` (`client/atmosphere.h`) sets them: a white sun over the Meadows,
a big red one low in the Ashlands' smoke, and a pale moon over the Crypt.

- Clouds are rows of a few low-poly balls, biggest in the middle, made from the land's seed, so a
  land's clouds are its own and the same each time. The wind blows them along at two and a half
  metres a second, through a square 900 metres wide around the camera: one that leaves it comes back
  in on the other side, after fading into the horizon. `cloudiness` says how many of a land's forty
  are out.
- Each face is lit by the sun from its own slope, worked out in the shader from how the position
  changes across the pixel, so the clouds are as faceted as everything else. A face smaller than a
  pixel has no slope to speak of, and is lit as if it faced up, as normalizing nothing gives NaN,
  which the bloom would spread over the whole picture.
- Their shadows drift over the land, too: the world's shader takes the 64 balls nearest to what the
  camera looks at, and for each pixel follows the line to the sun up to each ball's height, taking
  some of the sun's light away where it passes through one.
- The sea's waves and the clouds go by the renderer's own clock, which `Animate` moves on, rather
  than the window's, so that a film can run it slower (see [Cinema](cinema.md)).

## Post-processing

The client draws the world into a frame of its own, a render texture, and puts the frame on the
screen with the GUI on top, as raylib's
[post-processing example](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_postprocessing.c)
does, so that shaders can work on the whole picture first. `client/postprocess.h` has it.

- OpenGL keeps a texture's rows from the bottom up, so the frame goes to the screen with a negative
  height, which flips it the right way up.
- The sea and the skills' fx are see-through, and leave the frame see-through where they are, so the
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
- The lava and the ooze give off light of their own, their emission, which the shaders add to the
  sun's, most at the tops of their waves, and which takes them past white, so they bloom, as do the
  skills' fx. The screen cuts each channel at white on its own, which turns a bright orange
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
