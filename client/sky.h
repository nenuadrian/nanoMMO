#ifndef NANOMMO_CLIENT_SKY_H_
#define NANOMMO_CLIENT_SKY_H_

#include <raylib.h>

#include <cstdint>
#include <vector>

#include "client/atmosphere.h"

namespace nanommo {

/// The sky over a land: its colour from the horizon up, the sun, and low-poly
/// clouds that drift over with the wind, whose shadows drift over the ground
/// beneath them.
class Sky {
 public:
  /// The most clouds a land has, all of them on a cloudy day.
  static constexpr int kClouds = 40;
  /// The world's shader darkens the ground under this many of the clouds'
  /// balls, those nearest to what the camera looks at, as it can't afford
  /// all of them for each pixel.
  static constexpr int kShadows = 64;
  /// In metres a second, which way the wind blows the clouds.
  static constexpr Vector2 kWind = {2.5f, 1.0f};
  /// The clouds drift through a square this wide, in metres, around the
  /// camera, and come back in at the other side when they leave it.
  static constexpr float kSpread = 900.0f;

  Sky();
  ~Sky();

  Sky(const Sky&) = delete;
  Sky& operator=(const Sky&) = delete;

  /// The land's clouds, from its seed, so that a land's clouds are its own,
  /// and the same each time.
  void Gather(uint32_t seed);

  /// Draws the sky, and the atmosphere's share of the clouds where the wind
  /// has blown them by then, around the camera. Inside BeginMode3D, first.
  void Draw(const Camera3D& camera, const Atmosphere& atmosphere,
            float seconds) const;

  /// Tells the world's shader where the clouds' shadows are: the middle of
  /// each of their balls, its height, and how wide its shadow is.
  void Shade(Shader shader, const Camera3D& camera,
             const Atmosphere& atmosphere, float seconds) const;

 private:
  /// One of the low-poly balls a cloud is made of, from the cloud's middle,
  /// and its size along each axis, in metres.
  struct Puff {
    Vector3 offset;
    Vector3 size;
  };
  struct Cloud {
    Vector2 at;
    float height = 0.0f;
    /// Clouds come in this order as the sky gets cloudier: the first on a
    /// clear day, all of them on a cloudy one.
    float rank = 0.0f;
    std::vector<Puff> puffs;
  };

  /// Where the cloud is after the seconds of wind, in the square around the
  /// camera.
  Vector2 Blown(const Cloud& cloud, const Camera3D& camera,
                float seconds) const;

  std::vector<Cloud> clouds_;
  Shader dome_shader_;
  Shader cloud_shader_;
  Model dome_;
  Model puff_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SKY_H_
