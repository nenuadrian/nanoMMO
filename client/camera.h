#ifndef NANOMMO_CLIENT_CAMERA_H_
#define NANOMMO_CLIENT_CAMERA_H_

#include <raylib.h>

#include "terrain/terrain.h"

namespace nanommo {

/// Looking down at the middle of the board, from above and behind.
Camera3D GameCamera();

/// Follows your player around, from behind and above.
class FollowCamera {
 public:
  static constexpr float kNearest = 4.0f;
  static constexpr float kFarthest = 40.0f;

  /// Turns by the angle, in radians, and zooms in by the share of the
  /// distance, then looks at the player from there.
  void Update(Vector3 player, float turn, float zoom, const Terrain& terrain);

  /// Turns the keys' direction from the screen's to the world's, so that W
  /// always walks away from the camera.
  Vector3 Relative(Vector3 direction) const;

  const Camera3D& camera() const { return camera_; }

 private:
  float yaw_ = 0.0f;
  float distance_ = 15.0f;
  Camera3D camera_ = GameCamera();
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CAMERA_H_
