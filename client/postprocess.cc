#include "client/postprocess.h"

#include <raylib.h>
#include <rlgl.h>

namespace nanommo {

PostProcess::~PostProcess() { UnloadRenderTexture(frame_); }

void PostProcess::Begin() {
  // As big as the screen, made again when the window changes size.
  const int width = GetScreenWidth();
  const int height = GetScreenHeight();
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
  // with a negative height, which flips it the right way up.
  const Texture2D& texture = frame_.texture;
  DrawTextureRec(texture,
                 {0.0f, 0.0f, static_cast<float>(texture.width),
                  -static_cast<float>(texture.height)},
                 {0.0f, 0.0f}, WHITE);
  EndBlendMode();
}

}  // namespace nanommo
