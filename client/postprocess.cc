#include "client/postprocess.h"

#include <raylib.h>
#include <rlgl.h>

namespace nanommo {

PostProcess::~PostProcess() { UnloadRenderTexture(frame_); }

void PostProcess::Begin() {
  // In pixels, as a high-DPI screen has more of them than points. It's made
  // again when the window changes size.
  const int width = GetRenderWidth();
  const int height = GetRenderHeight();
  if (frame_.texture.width != width || frame_.texture.height != height) {
    UnloadRenderTexture(frame_);
    frame_ = LoadRenderTexture(width, height);
  }
  BeginTextureMode(frame_);
}

void PostProcess::End() { EndTextureMode(); }

void PostProcess::Draw() const {
  // The sea and the glows are see-through, and leave the frame see-through
  // where they are, so the frame replaces what's on the screen: blending it in
  // would let an older picture show through.
  rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM);
  // OpenGL keeps a texture's rows from the bottom up, so the frame is drawn
  // with a negative height, which flips it the right way up. We draw on the
  // screen in points, so the frame's pixels are fitted to the screen's points.
  const Texture2D& texture = frame_.texture;
  DrawTexturePro(texture,
                 {0.0f, 0.0f, static_cast<float>(texture.width),
                  -static_cast<float>(texture.height)},
                 {0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                  static_cast<float>(GetScreenHeight())},
                 {0.0f, 0.0f}, 0.0f, WHITE);
  EndBlendMode();
}

}  // namespace nanommo
