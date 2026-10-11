#include "client/offscreen.h"

namespace nanommo {

std::ostream& operator<<(std::ostream& out, const Pixel& pixel) {
  return out << "rgba(" << pixel.r << ", " << pixel.g << ", " << pixel.b << ", "
             << pixel.a << ")";
}

Offscreen::Offscreen(int width, int height)
    : target_(LoadRenderTexture(width, height)) {}

Offscreen::~Offscreen() { UnloadRenderTexture(target_); }

void Offscreen::Render(const Renderer& renderer) {
  BeginTextureMode(target_);
  renderer.Draw();
  EndTextureMode();
}

Pixel Offscreen::ReadPixel(int x, int y) const {
  Image image = LoadImageFromTexture(target_.texture);
  // OpenGL stores a texture's rows from the bottom up, so we flip the image.
  ImageFlipVertical(&image);
  const Color color = GetImageColor(image, x, y);
  UnloadImage(image);
  return {color.r, color.g, color.b, color.a};
}

}  // namespace nanommo
