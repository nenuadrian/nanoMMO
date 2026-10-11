#include <imgui.h>
#include <raylib.h>

#include <iostream>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "client/gui.h"
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
  while (!window->ShouldClose()) {
    BeginDrawing();
    ClearBackground(Color{18, 20, 31, 255});
    gui.Begin();
    ImGui::ShowDemoWindow();
    gui.End();
    EndDrawing();
  }
  return 0;
}
