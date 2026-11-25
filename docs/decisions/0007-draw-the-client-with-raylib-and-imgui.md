# 7. Draw the client with raylib, and its interface with Dear ImGui

## Status

Accepted

## Context

Episode 2 was written again on 11 October 2026, after its first version drew with Vulkan
([0006](0006-draw-the-client-with-vulkan.md)). The series is about the engineering around the
game, so the graphics code should stay small, and the time go on the server, the tests and the
deployment.

## Decision

- raylib, fetched by CMake, for the window, the keyboard and the mouse, drawing in 2D and 3D, models
  and their animations, and later the audio. It draws with OpenGL, and needs no more than 3.3 of
  it. Our shaders are GLSL 330, as raw strings in the C++.
- Dear ImGui, through rlImGui, for every window. It's immediate mode. Each frame, a call for each
  widget draws it and says whether it was used, so a window is plain code. rlImGui has no releases,
  so it's pinned to a commit.
- When we want to go lower, raylib's own lower layer, rlgl, is close to OpenGL.

## Alternatives considered

- OpenGL directly, which is easy to start with, as the driver does a lot of the work (episode 2).
- Vulkan, the modern, cross-platform API, which is a lot more explicit and gives more control over
  performance, and takes a few hundred lines of setup before the most basic shape is drawn. It
  was the first version ([0006](0006-draw-the-client-with-vulkan.md)).
- An engine, such as Unity, Unreal or Godot. They give a lot, with an editor and a way of working
  of their own, and the engineering we want to look at would end up hidden inside them
  (episode 2).

## Consequences

- At `ep002/end`, the client is about 550 lines, where the Vulkan one was about 1,500 for less
  ([0006](0006-draw-the-client-with-vulkan.md)).
- OpenGL ties us to an API Apple stopped updating years ago, for Metal. It still works on today's
  Macs, and if that changes, raylib supports OpenGL ES, which layers such as ANGLE run on Metal.
- raylib needs a window for an OpenGL context, even to draw into a texture, so the tests open a
  hidden one, and in Docker run on Xvfb ([0004](0004-build-test-and-check-in-docker.md)). OpenGL
  keeps a texture's rows bottom up, so reading pixels back flips them (episode 2).
- raylib's ways bit us now and then. It skins on the CPU unless built with `SUPPORT_GPU_SKINNING`
  (episode 7), keeps the first 31 characters of a clip's name, and binds a mesh where its bones
  rest, which Quaternius's skeleton needed its inverse bind matrices read for (episode 10). In
  episode 28 LeakSanitizer found what it left behind unloading a model with bones, and 6.2's
  `UnloadModel` frees that itself, so the client's own free became a double free (0.59.0).
- ImGui carried the interface from a debug menu, in episode 2, to the game's own look, with panels,
  fonts and a scale that follows the window, in 0.63.0, without a second UI library.
- The post-processing, bloom, cel shading and FXAA, is raylib's render textures and our shaders
  (episode 33, [Graphics and sound](../graphics.md)).
