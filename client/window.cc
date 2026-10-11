#include "client/window.h"

#include <raylib.h>

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/status.h"

namespace nanommo {

absl::StatusOr<std::unique_ptr<Window>> Window::Create(const std::string& title,
                                                       int width, int height) {
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(width, height, title.c_str());
  if (!IsWindowReady()) {
    return absl::UnavailableError("couldn't open the window");
  }
  // raylib closes the window on Escape. In a game, Escape closes menus.
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);
  return absl::WrapUnique(new Window());
}

Window::~Window() { CloseWindow(); }

bool Window::ShouldClose() const { return WindowShouldClose(); }

}  // namespace nanommo
