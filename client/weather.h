#ifndef NANOMMO_CLIENT_WEATHER_H_
#define NANOMMO_CLIENT_WEATHER_H_

#include <raylib.h>

#include <optional>

#include "client/atmosphere.h"
#include "protocol/nanommo.pb.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What the sky lets fall, drawn around the camera from the sky the server
/// sent. Each drop is worked out from the clock alone, where it is in its
/// fall, so nothing is kept for any of them between frames.
class Weather {
 public:
  Weather();
  ~Weather();

  Weather(const Weather&) = delete;
  Weather& operator=(const Weather&) = delete;

  /// The land's ground, which what falls lands on, and its place in the
  /// lands, which says what falls there.
  void set_land(const Terrain& terrain, int land);

  /// Moves on by the seconds, under the sky. Off, nothing falls, and the
  /// ground dries.
  void Update(float seconds, const protocol::Sky& sky, bool on);

  /// Inside BeginMode3D, after what's solid and the sea, as it's all
  /// see-through.
  void Draw(const Camera3D& camera, const Atmosphere& atmosphere) const;

  /// From dry to soaked, which rain makes it, and the sun dries.
  float wetness() const { return wetness_.value_or(0.0f); }

 private:
  void DrawFalling(const Camera3D& camera, const Atmosphere& atmosphere,
                   Vector3 middle) const;

  Shader shader_;
  std::optional<Terrain> terrain_;
  int land_ = 0;
  protocol::Sky sky_;
  bool on_ = true;
  /// The seconds the weather's moved on by, which everything falls by.
  float clock_ = 0.0f;
  /// Unset until the first sky, so that rain we come into is already wet.
  std::optional<float> wetness_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_WEATHER_H_
