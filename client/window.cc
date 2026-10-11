#include "client/window.h"

#include <raylib.h>

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/status.h"

namespace nanommo {

absl::StatusOr<std::unique_ptr<Window>> Window::Create(const std::string& title,
                                                       int width, int height,
                                                       Visibility visibility) {
  SetTraceLogLevel(LOG_WARNING);
  unsigned int flags = FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT;
  if (visibility == Visibility::kHidden) {
    flags |= FLAG_WINDOW_HIDDEN;
  }
  SetConfigFlags(flags);
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

bool Window::ShouldClose() const { return closing_ || WindowShouldClose(); }

void Window::Close() { closing_ = true; }

}  // namespace nanommo
