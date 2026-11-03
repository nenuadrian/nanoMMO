#ifndef NANOMMO_CLIENT_MAP_H_
#define NANOMMO_CLIENT_MAP_H_

#include <raylib.h>

#include <array>
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

/// How the minimap's view is turned: north up, or with what's ahead of the
/// camera up, as the player chooses.
enum class MinimapTurn { kNorthUp, kWithCamera };

/// A land seen from above, north up: its ground in the land's colours,
/// shaded by its slopes, its sea and its trees, drawn once into a texture as
/// the land is built. The minimap, round, in the top right, shows what's
/// around you, and the map, on M, the whole land, with whoever's near you on
/// both.
class LandMap {
 public:
  /// In metres from you to the minimap's rim, at each of its zooms.
  static constexpr std::array<float, 3> kZooms = {25.0f, 45.0f, 80.0f};

  LandMap() = default;
  ~LandMap();

  LandMap(const LandMap&) = delete;
  LandMap& operator=(const LandMap&) = delete;

  /// Draws the land into the texture, in place of the last one.
  void Build(const Terrain& terrain, int land);

  /// The minimap, with the monster you picked, by its id, and where you're
  /// walking to, if you are, under the land's name, turned to the heading the
  /// camera looks along if the player chose that. Scrolling over it, or its
  /// buttons, zoom in and out. Returns true when its map button's clicked.
  bool DrawMinimap(const std::vector<Figure>& figures, uint32_t target,
                   std::optional<Vector3> destination, float heading,
                   MinimapTurn turn);

  /// How far down the screen the minimap reaches, in points, for what goes
  /// under it.
  float minimap_bottom() const { return bottom_; }

  int zoom() const { return zoom_; }
  void set_zoom(int zoom);

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
  int zoom_ = 1;
  float bottom_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MAP_H_
