#ifndef NANOMMO_CLIENT_WINDOW_H_
#define NANOMMO_CLIENT_WINDOW_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"

namespace nanommo {

class Window {
 public:
  static absl::StatusOr<std::unique_ptr<Window>> Create(
      const std::string& title, int width, int height);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool ShouldClose() const;

 private:
  Window() = default;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_WINDOW_H_
