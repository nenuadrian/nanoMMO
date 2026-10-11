#include <raylib.h>

#include <iostream>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "client/gui.h"
#include "client/menu.h"
#include "client/renderer.h"
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

  while (!window->ShouldClose()) {
    BeginDrawing();
    renderer.Draw();
    gui.Begin();
    menu.Build(*window, *renderer.mutable_background());
    gui.End();
    EndDrawing();
  }
  return 0;
}
