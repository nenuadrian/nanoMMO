# Monsters

The monsters the client draws. Unlike the characters, they're in git, as their licence lets anyone
share them.

| File | What | From | Licence |
|---|---|---|---|
| `skeleton.glb` | Skeleton, by [Quaternius](https://quaternius.com), with an idle, a run, an attack, a death and a spawn | [Poly Pizza](https://poly.pizza/m/wODZYCgX5Z), as glTF | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/), public domain |

The file is as it comes from Poly Pizza. CC0 doesn't ask for credit, but Quaternius made it, so
here it is.

The client loads it as it is: it scales the skeleton to the height of each kind whose look names it,
in `content/monsters.txtpb`, finds the clips by the word in
their names, and walks with the run, played at 0.4 of its speed. It reads the bind pose from the
file's inverse bind matrices too, as raylib binds the mesh to where the bones rest, and the
skeleton's arms rest about 70 degrees from where its mesh was bound.
