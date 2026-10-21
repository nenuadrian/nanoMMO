#include "client/postprocess.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>

namespace nanommo {

namespace {

// Keeps what's past white, from none of a colour whose brightest channel is
// just at it, to all of one at twice as bright.
constexpr const char* kBrightShader = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
out vec4 finalColor;

void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb;
  float brightest = max(color.r, max(color.g, color.b));
  finalColor = vec4(color * clamp(brightest - 1.0, 0.0, 1.0), 1.0);
}
)";

// Spreads a half as it doubles, over the nine of its pixels around each one,
// so that the light fades out smoothly, where doubling alone would leave
// blocks.
constexpr const char* kSpreadShader = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 pixel;
out vec4 finalColor;

void main() {
  vec3 sum = vec3(0.0);
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      float weight = (2.0 - abs(float(x))) * (2.0 - abs(float(y)));
      sum += texture(texture0, fragTexCoord + vec2(x, y) * pixel).rgb * weight;
    }
  }
  finalColor = vec4(sum / 16.0, 1.0);
}
)";

// The frame, with the bloom added on top.
constexpr const char* kCombineShader = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D bloom;
uniform float strength;
out vec4 finalColor;

void main() {
  vec3 color = texture(texture0, fragTexCoord).rgb;
  color += texture(bloom, fragTexCoord).rgb * strength;
  finalColor = vec4(color, 1.0);
}
)";

// How much of the bloom goes on the frame. The halves add up, so it's a
// fraction.
constexpr float kStrength = 0.25f;

// Like raylib's LoadRenderTexture, with half a float to each of a pixel's
// channels, which can go past white, where raylib's eight bits stop at it.
RenderTexture2D LoadFrame(int width, int height) {
  constexpr int kFormat = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
  RenderTexture2D frame{};
  frame.id = rlLoadFramebuffer();
  frame.texture = {.id = rlLoadTexture(nullptr, width, height, kFormat, 1),
                   .width = width,
                   .height = height,
                   .mipmaps = 1,
                   .format = kFormat};
  frame.depth = {.id = rlLoadTextureDepth(width, height, true),
                 .width = width,
                 .height = height};
  rlFramebufferAttach(frame.id, frame.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                      RL_ATTACHMENT_TEXTURE2D, 0);
  rlFramebufferAttach(frame.id, frame.depth.id, RL_ATTACHMENT_DEPTH,
                      RL_ATTACHMENT_RENDERBUFFER, 0);
  return frame;
}

// Draws a texture over the whole of a target. OpenGL keeps a texture's rows
// from the bottom up, so it's drawn with a negative height, which flips it the
// right way up.
void DrawOver(const Texture2D& texture, float width, float height) {
  DrawTexturePro(texture,
                 {0.0f, 0.0f, static_cast<float>(texture.width),
                  -static_cast<float>(texture.height)},
                 {0.0f, 0.0f, width, height}, {0.0f, 0.0f}, 0.0f, WHITE);
}

void DrawInto(const RenderTexture2D& target, const Texture2D& texture) {
  BeginTextureMode(target);
  DrawOver(texture, static_cast<float>(target.texture.width),
           static_cast<float>(target.texture.height));
  EndTextureMode();
}

}  // namespace

// raylib's own vertex shader passes the texture's coordinates on.
PostProcess::PostProcess()
    : bright_(LoadShaderFromMemory(nullptr, kBrightShader)),
      spread_(LoadShaderFromMemory(nullptr, kSpreadShader)),
      combine_(LoadShaderFromMemory(nullptr, kCombineShader)) {}

PostProcess::~PostProcess() {
  UnloadRenderTexture(frame_);
  for (const RenderTexture2D& half : halves_) {
    UnloadRenderTexture(half);
  }
  UnloadShader(bright_);
  UnloadShader(spread_);
  UnloadShader(combine_);
}

void PostProcess::Begin() {
  // In pixels, as a high-DPI screen has more of them than points. It's made
  // again when the window changes size.
  const int width = GetRenderWidth();
  const int height = GetRenderHeight();
  if (frame_.texture.width != width || frame_.texture.height != height) {
    Resize(width, height);
  }
  BeginTextureMode(frame_);
}

void PostProcess::Resize(int width, int height) {
  UnloadRenderTexture(frame_);
  frame_ = LoadFrame(width, height);
  // Halving a texture with the bilinear filter averages each two by two block
  // of its pixels.
  SetTextureFilter(frame_.texture, TEXTURE_FILTER_BILINEAR);
  for (RenderTexture2D& half : halves_) {
    width = std::max(width / 2, 1);
    height = std::max(height / 2, 1);
    UnloadRenderTexture(half);
    half = LoadFrame(width, height);
    SetTextureFilter(half.texture, TEXTURE_FILTER_BILINEAR);
  }
}

void PostProcess::End() { EndTextureMode(); }

void PostProcess::Draw() {
  // What's bright, in a frame half the size.
  BeginShaderMode(bright_);
  DrawInto(halves_[0], frame_.texture);
  EndShaderMode();
  // Halved again and again, so that the smallest half spreads its light the
  // widest, for few pixels.
  for (int i = 1; i < kHalves; ++i) {
    DrawInto(halves_[i], halves_[i - 1].texture);
  }
  // And added back up, from the smallest, each spread as it doubles.
  BeginBlendMode(BLEND_ADDITIVE);
  BeginShaderMode(spread_);
  for (int i = kHalves - 1; i > 0; --i) {
    const Texture2D& texture = halves_[i].texture;
    const Vector2 pixel = {1.0f / static_cast<float>(texture.width),
                           1.0f / static_cast<float>(texture.height)};
    SetShaderValue(spread_, GetShaderLocation(spread_, "pixel"), &pixel,
                   SHADER_UNIFORM_VEC2);
    DrawInto(halves_[i - 1], texture);
  }
  EndShaderMode();
  EndBlendMode();

  // The frame to the screen, with the bloom on top, fitted to its points, as
  // raylib draws on the screen in points. The sea and the glows leave the
  // frame see-through where they are, and the combine shader writes it opaque,
  // so that it covers what's on the screen.
  BeginShaderMode(combine_);
  SetShaderValueTexture(combine_, GetShaderLocation(combine_, "bloom"),
                        halves_[0].texture);
  SetShaderValue(combine_, GetShaderLocation(combine_, "strength"), &kStrength,
                 SHADER_UNIFORM_FLOAT);
  DrawOver(frame_.texture, static_cast<float>(GetScreenWidth()),
           static_cast<float>(GetScreenHeight()));
  EndShaderMode();
}

}  // namespace nanommo
