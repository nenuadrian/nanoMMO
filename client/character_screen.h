#ifndef NANOMMO_CLIENT_CHARACTER_SCREEN_H_
#define NANOMMO_CLIENT_CHARACTER_SCREEN_H_

#include <vector>

#include "client/animator.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

class CharacterScreen {
 public:
  CharacterScreen() = default;

  CharacterScreen(const CharacterScreen&) = delete;
  CharacterScreen& operator=(const CharacterScreen&) = delete;

  bool* mutable_visible() { return &visible_; }
  void Toggle() { visible_ = !visible_; }

  void Render(const Renderer& renderer);

  void Build(const std::vector<Figure>& figures, const protocol::World& world);

 private:
  static constexpr int kPreviewPixels = 360;

  bool visible_ = false;
  Offscreen preview_{kPreviewPixels, kPreviewPixels};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTER_SCREEN_H_
