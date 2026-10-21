#ifndef NANOMMO_CLIENT_POSTPROCESS_H_
#define NANOMMO_CLIENT_POSTPROCESS_H_

#include <raylib.h>

#include <array>

#include "client/graphics.h"

namespace nanommo {

/// What the world goes through on its way to the screen. The renderer draws
/// it into a frame of our own, a render texture, and the frame goes to the
/// screen from there, so that shaders can work on the whole picture first. The
/// GUI goes on top, as it is.
class PostProcess {
 public:
  PostProcess();
  ~PostProcess();

  PostProcess(const PostProcess&) = delete;
  PostProcess& operator=(const PostProcess&) = delete;

  /// What's drawn from here goes into the frame, until End, and Draw puts it
  /// on the screen as the settings say.
  void Begin(const GraphicsSettings& graphics);
  void End();

  /// Draws the frame to the screen, with bloom if it's on.
  void Draw();

 private:
  /// The bloom's halves of the frame, each half the size of the one before.
  static constexpr int kHalves = 5;

  void Resize(int width, int height);
  void Bloom();
  /// Draws the frame with the bloom on top, at a width and a height.
  void Combine(float width, float height);

  Shader bright_;
  Shader spread_;
  Shader combine_;
  Shader fxaa_;
  GraphicsSettings graphics_;
  RenderTexture2D frame_{};
  std::array<RenderTexture2D, kHalves> halves_{};
  /// The frame with its bloom, as the screen would show it, for FXAA.
  RenderTexture2D picture_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_POSTPROCESS_H_
