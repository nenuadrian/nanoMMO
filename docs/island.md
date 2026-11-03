# The island

Each land's island comes from a seed, a number: the game server's `NANOMMO_WORLD_SEED`, 1 by
default, plus the land's own (see [Lands](worlds.md#lands)). The server sends it to each client with the
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
pixel, from how far it is, so the far sea fades too. The Sky page of the settings changes the
atmosphere as you play, with presets for dusk, night, mist and ash, and Reset puts the land's back.

The tree is Poly by Google's, from [Poly Pizza](https://poly.pizza/m/6pwiq7hSrHr), under
[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/), in the repo, as it was downloaded, in
`assets/trees` (see [assets/trees/README.md](../assets/trees/README.md)). Its texture is a palette of
four colours, so the client reads the model as triangles, and gives each face the colour under its
middle.

## Drawing the island fast

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
