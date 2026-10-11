#include "client/renderer.h"

#include <raylib.h>

namespace nanommo {

void Renderer::Draw() const {
  ClearBackground(ColorFromNormalized(
      {background_[0], background_[1], background_[2], 1.0f}));
}

}  // namespace nanommo
