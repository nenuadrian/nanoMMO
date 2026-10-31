#ifndef NANOMMO_CLIENT_MAP_H_
#define NANOMMO_CLIENT_MAP_H_

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "client/animator.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace nanommo {

/// The land from above, north up, a pixel a metre: its ground in the land's
/// colours, shaded by its slopes, its sea and its trees.
Image DrawLand(const Terrain& terrain, int land);

/// A land seen from above, north up: its ground in the land's colours,
/// shaded by its slopes, its sea and its trees, drawn once into a texture as
/// the land is built. The minimap in the bottom right shows what's around
/// you, and the map, on M, the whole land, with whoever's near you on both.
class LandMap {
 public:
  /// In metres from you to each side of the minimap.
  static constexpr float kAround = 45.0f;

  LandMap() = default;
  ~LandMap();

  LandMap(const LandMap&) = delete;
  LandMap& operator=(const LandMap&) = delete;

  /// Draws the land into the texture, in place of the last one.
  void Build(const Terrain& terrain, int land);

  /// The minimap, with the monster you picked, by its id, and where you're
  /// walking to, if you are.
  void DrawMinimap(const std::vector<Figure>& figures, uint32_t target,
                   std::optional<Vector3> destination) const;

  /// The map, in a window of its own, while open is true; its close button
  /// sets it to false. Returns the place you clicked on, to walk to.
  std::optional<Place> DrawMap(const std::vector<Figure>& figures,
                               uint32_t target,
                               std::optional<Vector3> destination,
                               bool* open) const;

 private:
  Texture2D texture_{};
  float half_ = 0.0f;
  int land_ = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MAP_H_
