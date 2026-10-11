#include <raylib.h>
#include <raymath.h>

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "client/gui.h"
#include "client/input.h"
#include "client/menu.h"
#include "client/renderer.h"
#include "client/scene.h"
#include "client/window.h"

int main() {
  absl::StatusOr<std::unique_ptr<nanommo::Window>> window_or =
      nanommo::Window::Create("nanoMMO", 1280, 720);
  if (!window_or.ok()) {
    std::cerr << "nanommo client: " << window_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Window> window = *std::move(window_or);
  nanommo::Gui gui;
  nanommo::Menu menu;
  nanommo::Renderer renderer;

  Vector3 position{0.0f, 0.5f, 0.0f};
  // Tiles a second.
  constexpr float kSpeed = 4.0f;

  while (!window->ShouldClose()) {
    position += Vector3Normalize(nanommo::Movement()) * kSpeed * GetFrameTime();
    std::vector<nanommo::Cube> cubes = nanommo::Ground();
    cubes.push_back({.position = position, .color = Color{242, 140, 51, 255}});
    renderer.set_cubes(std::move(cubes));

    BeginDrawing();
    renderer.Draw();
    gui.Begin();
    menu.Build(*window, *renderer.mutable_background());
    gui.End();
    EndDrawing();
  }
  return 0;
}
