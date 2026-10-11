#include <raylib.h>

#include <array>
#include <iostream>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "client/gui.h"
#include "client/menu.h"
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
  std::array<float, 3> background = {0.07f, 0.08f, 0.12f};

  while (!window->ShouldClose()) {
    BeginDrawing();
    ClearBackground(ColorFromNormalized(
        {background[0], background[1], background[2], 1.0f}));
    gui.Begin();
    menu.Build(*window, background);
    gui.End();
    EndDrawing();
  }
  return 0;
}
