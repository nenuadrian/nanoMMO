#ifndef NANOMMO_CLIENT_POSTPROCESS_H_
#define NANOMMO_CLIENT_POSTPROCESS_H_

#include <raylib.h>

namespace nanommo {

/// What the world goes through on its way to the screen. The renderer draws
/// it into a frame of our own, a render texture, and the frame goes to the
/// screen from there, so that shaders can work on the whole picture first. The
/// GUI goes on top, as it is.
class PostProcess {
 public:
  PostProcess() = default;
  ~PostProcess();

  PostProcess(const PostProcess&) = delete;
  PostProcess& operator=(const PostProcess&) = delete;

  /// What's drawn from here goes into the frame, until End.
  void Begin();
  void End();

  /// Draws the frame to the screen.
  void Draw() const;

 private:
  RenderTexture2D frame_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_POSTPROCESS_H_
