#ifndef NANOMMO_CLIENT_GRAPHICS_H_
#define NANOMMO_CLIENT_GRAPHICS_H_

namespace nanommo {

/// How the world looks on its way to the screen, as the player sets it in the
/// settings window.
struct GraphicsSettings {
  bool bloom = true;
  /// The sun's light in bands, and outlines, like a cartoon. A look like that
  /// is the player's to choose, so it starts off.
  bool cel_shading = false;
  /// Smooths the world's edges, with FXAA.
  bool antialias = true;

  bool operator==(const GraphicsSettings&) const = default;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GRAPHICS_H_
