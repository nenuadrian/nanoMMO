#include "client/postprocess.h"

#include <raylib.h>
#include <rlgl.h>

namespace nanommo {

namespace {

// raylib's example's bloom: each pixel gets the average of the 25 around it,
// spread two and a half pixels apart, added to it.
constexpr const char* kBloomShader = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 size;
out vec4 finalColor;

void main() {
  vec2 spread = 2.5 / size;
  vec4 sum = vec4(0.0);
  for (int x = -2; x <= 2; x++) {
    for (int y = -2; y <= 2; y++) {
      sum += texture(texture0, fragTexCoord + vec2(x, y) * spread);
    }
  }
  vec4 source = texture(texture0, fragTexCoord);
  finalColor = (sum / 25.0 + source) * colDiffuse;
}
)";

}  // namespace

// raylib's own vertex shader passes the texture's coordinates on.
PostProcess::PostProcess()
    : bloom_(LoadShaderFromMemory(nullptr, kBloomShader)) {}

PostProcess::~PostProcess() {
  UnloadRenderTexture(frame_);
  UnloadShader(bloom_);
}

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
  const Texture2D& texture = frame_.texture;
  // The example writes its frame's size into the shader, as 800 by 450, and
  // ours is the screen's.
  const Vector2 size = {static_cast<float>(texture.width),
                        static_cast<float>(texture.height)};
  SetShaderValue(bloom_, GetShaderLocation(bloom_, "size"), &size,
                 SHADER_UNIFORM_VEC2);
  // The sea and the glows are see-through, and leave the frame see-through
  // where they are, so the frame replaces what's on the screen: blending it in
  // would let an older picture show through.
  rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM);
  BeginShaderMode(bloom_);
  // OpenGL keeps a texture's rows from the bottom up, so the frame is drawn
  // with a negative height, which flips it the right way up. We draw on the
  // screen in points, so the frame's pixels are fitted to the screen's points.
  DrawTexturePro(texture,
                 {0.0f, 0.0f, static_cast<float>(texture.width),
                  -static_cast<float>(texture.height)},
                 {0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                  static_cast<float>(GetScreenHeight())},
                 {0.0f, 0.0f}, 0.0f, WHITE);
  EndShaderMode();
  EndBlendMode();
}

}  // namespace nanommo
