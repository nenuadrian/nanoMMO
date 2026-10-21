#include "client/camera.h"

#include <algorithm>
#include <cmath>

namespace nanommo {

namespace {

// Up to the player's chest, so that they're in the middle of the screen.
constexpr float kChest = 1.2f;

constexpr float kAboveGround = 1.0f;

// Past straight down, the camera would go over the top of the player and look
// back the other way. Near level, it looks along the ground, and any rise in
// between hides the player.
constexpr float kLowest = 5.0f * DEG2RAD;
constexpr float kHighest = 80.0f * DEG2RAD;

}  // namespace

Camera3D GameCamera() {
  return {.position = {0.0f, 11.0f, 10.0f},
          .target = {0.0f, 0.0f, 0.0f},
          .up = {0.0f, 1.0f, 0.0f},
          .fovy = 50.0f,
          .projection = CAMERA_PERSPECTIVE};
}

void FollowCamera::Update(Vector3 player, float turn, float tilt, float zoom,
                          const Terrain& terrain) {
  yaw_ += turn;
  pitch_ = std::clamp(pitch_ + tilt, kLowest, kHighest);
  distance_ = std::clamp(distance_ * (1.0f - zoom), kNearest, kFarthest);

  const Vector3 target{player.x, player.y + kChest, player.z};
  Vector3 position{target.x + distance_ * std::cos(pitch_) * std::sin(yaw_),
                   target.y + distance_ * std::sin(pitch_),
                   target.z + distance_ * std::cos(pitch_) * std::cos(yaw_)};
  // Up a hill behind the player, it would see the inside of the hill.
  position.y = std::max(position.y,
                        terrain.Height(position.x, position.z) + kAboveGround);
  camera_.position = position;
  camera_.target = target;
}

Vector3 FollowCamera::Relative(Vector3 direction) const {
  const float cos = std::cos(yaw_);
  const float sin = std::sin(yaw_);
  return {direction.x * cos + direction.z * sin, 0.0f,
          direction.z * cos - direction.x * sin};
}

}  // namespace nanommo
