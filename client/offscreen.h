#ifndef NANOMMO_CLIENT_OFFSCREEN_H_
#define NANOMMO_CLIENT_OFFSCREEN_H_

#include <raylib.h>

#include <ostream>

#include "client/animator.h"
#include "client/renderer.h"

namespace nanommo {

struct Pixel {
  int r = 0;
  int g = 0;
  int b = 0;
  int a = 0;

  bool operator==(const Pixel&) const = default;
};

std::ostream& operator<<(std::ostream& out, const Pixel& pixel);

class Offscreen {
 public:
  Offscreen(int width, int height);
  ~Offscreen();

  Offscreen(const Offscreen&) = delete;
  Offscreen& operator=(const Offscreen&) = delete;

  void Render(const Renderer& renderer);

  void RenderPreview(const Renderer& renderer, const Figure& figure,
                     const Camera3D& camera);

  Texture2D texture() const { return target_.texture; }

  Pixel ReadPixel(int x, int y) const;

 private:
  RenderTexture2D target_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_OFFSCREEN_H_
