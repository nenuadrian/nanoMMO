#ifndef NANOMMO_CLIENT_RENDERER_H_
#define NANOMMO_CLIENT_RENDERER_H_

#include <array>

namespace nanommo {

class Renderer {
 public:
  void Draw() const;

  const std::array<float, 3>& background() const { return background_; }
  void set_background(const std::array<float, 3>& background) {
    background_ = background;
  }
  /// For the settings window, which edits the colour in place.
  std::array<float, 3>* mutable_background() { return &background_; }

 private:
  std::array<float, 3> background_ = {0.07f, 0.08f, 0.12f};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
