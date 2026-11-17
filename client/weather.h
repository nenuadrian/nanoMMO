#ifndef NANOMMO_CLIENT_WEATHER_H_
#define NANOMMO_CLIENT_WEATHER_H_

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "client/atmosphere.h"
#include "protocol/nanommo.pb.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What the sky lets fall, what glows in the dark, and the lightning, drawn
/// around the camera from the sky the server sent. Each drop, flake and ember
/// is worked out from the clock alone, where it is in its fall, so nothing is
/// kept for any of them between frames.
class Weather {
 public:
  Weather();
  ~Weather();

  Weather(const Weather&) = delete;
  Weather& operator=(const Weather&) = delete;

  /// The land's ground, which what falls lands on, and its place in the
  /// lands, which says what falls there and what glows.
  void set_land(const Terrain& terrain, int land);

  /// Where the lightning strikes and how often comes from this, so that a
  /// film's bolts strike where they did the last time.
  void set_seed(uint32_t seed) { seed_ = seed; }

  /// As soaked as this to start with, rather than as wet as the weather
  /// would make it: for a film that starts after the rain.
  void set_wetness(float wetness) { wetness_ = wetness; }

  /// Moves on by the seconds, under the sky, around the camera. Off, nothing
  /// falls or glows, and the ground dries.
  void Update(float seconds, const protocol::Sky& sky, bool on,
              const Camera3D& camera);

  /// Inside BeginMode3D, after what's solid and the sea, as it's all
  /// see-through.
  void Draw(const Camera3D& camera, const Atmosphere& atmosphere) const;

  /// From dry to soaked, which rain makes it, and the sun dries.
  float wetness() const { return wetness_.value_or(0.0f); }

  /// How bright the sky is from a bolt that's just struck, from 0 to 1.
  float flash() const;

 private:
  struct Strike {
    std::vector<Vector3> points;
    std::vector<Vector3> branch;
    float at = -10.0f;
    float near = 1.0f;
  };

  void DrawFalling(const Camera3D& camera, const Atmosphere& atmosphere,
                   Vector3 middle) const;
  void DrawGlows(const Camera3D& camera, Vector3 middle) const;
  void DrawStrike(const Camera3D& camera) const;
  float StrikeLight() const;

  Shader shader_;
  std::optional<Terrain> terrain_;
  int land_ = 0;
  uint32_t seed_ = 1;
  protocol::Sky sky_;
  bool on_ = true;
  /// The seconds the weather's moved on by, which everything falls by.
  float clock_ = 0.0f;
  /// Unset until the first sky, so that rain we come into is already wet.
  std::optional<float> wetness_;
  /// Until the next bolt, in seconds.
  float next_bolt_ = 3.0f;
  uint32_t bolts_struck_ = 0;
  Strike strike_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_WEATHER_H_
