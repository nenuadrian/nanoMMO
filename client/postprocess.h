#ifndef NANOMMO_CLIENT_POSTPROCESS_H_
#define NANOMMO_CLIENT_POSTPROCESS_H_

#include <raylib.h>

#include <array>

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

  /// What's drawn from here goes into the frame, until End.
  void Begin();
  void End();

  /// Draws the frame to the screen, with bloom.
  void Draw();

 private:
  /// The bloom's halves of the frame, each half the size of the one before.
  static constexpr int kHalves = 5;

  void Resize(int width, int height);

  Shader bright_;
  Shader spread_;
  Shader combine_;
  RenderTexture2D frame_{};
  std::array<RenderTexture2D, kHalves> halves_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_POSTPROCESS_H_
