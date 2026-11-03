# Changes

What changed in each version of nanoMMO, the newest first.

A version is MAJOR.MINOR.PATCH, from `project()` in `CMakeLists.txt`. While the major version is
0, each episode's code is a minor version, 0.59.0 for episode 59's, and a patch fixes one without
changing what the client and the servers say to each other. The login server lets in a client of
its own major and minor version, and turns any other away.

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
