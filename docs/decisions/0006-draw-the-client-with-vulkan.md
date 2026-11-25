# 6. Draw the client with Vulkan

## Status

Superseded by [0007](0007-draw-the-client-with-raylib-and-imgui.md)

## Context

Episode 2, as first written on 10 October 2026, started the client, with a window, a frame loop, a
user interface, and a test that renders a frame without a screen. The client needed a way to talk
to the graphics card. Why Vulkan was picked then isn't recorded.

## Decision

- A GLFW window, made without an OpenGL context, as Vulkan draws into it.
- Vulkan through vk-bootstrap. The client had an instance with the validation layers in debug
  builds, a GPU with Vulkan 1.3, dynamic rendering and synchronization2, a swapchain, and two
  frames in flight, each with its own command buffer, semaphores and fence. The Vulkan headers and
  vk-bootstrap were pinned to 1.4.341, and only the loader came from the system, through the Vulkan
  SDK on macOS and Windows, and `libvulkan-dev` on Ubuntu.
- Dear ImGui through its GLFW and Vulkan backends, and GLM for the matrices.
- Shaders in GLSL, compiled to SPIR-V by glslc as the client builds.
- The tests rendered offscreen on lavapipe, Mesa's Vulkan on the CPU, in Docker.

## Alternatives considered

Not recorded for this version. [0007](0007-draw-the-client-with-raylib-and-imgui.md) lists what
was weighed when it was replaced.

## Consequences

- Vulkan is explicit. The client moved each swapchain image into a layout it could draw into and
  on to be presented, waited on fences for the GPU, and rebuilt the swapchain whenever the window
  was resized. The first offscreen test came back with red and blue swapped, as the swapchain's
  format stores each pixel blue first.
- Where a cube moved over a board of tiles with W, A, S and D, under ImGui's menu, with an
  offscreen test, the client was about 1,500 lines of sources and shaders, without its tests. At
  `ep002/end`, raylib's does that, with a lit cube, in about 550.
- The series is about the engineering around the game, so its graphics code should stay small.
  On 11 October 2026, episodes 2 to 4 were written again on raylib
  ([0007](0007-draw-the-client-with-raylib-and-imgui.md)).
- The Vulkan history isn't on `main`. The `vulkan-series` branch keeps it, through what were the
  first four episodes, and nothing builds on it.
