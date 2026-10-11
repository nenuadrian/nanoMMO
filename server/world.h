#ifndef NANOMMO_SERVER_WORLD_H_
#define NANOMMO_SERVER_WORLD_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace nanommo {

enum class Motion { kIdle, kWalk, kRun };

struct Player {
  std::string name;
  float x = 0.0f;
  float z = 0.0f;
  /// In radians, around the vertical. Zero faces along z, towards the camera.
  float yaw = 0.0f;
  Motion motion = Motion::kIdle;
  float input_x = 0.0f;
  float input_z = 0.0f;
  bool input_run = false;
};

class World {
 public:
  /// In tiles a second. A tile is a metre, and that's the walk clip's speed.
  static constexpr float kWalkSpeed = 1.8f;

  static constexpr float kRunSpeed = 3.1f;

  /// Six tiles to the ground's edge, less half a tile, so players stay on it.
  static constexpr float kEdge = 5.5f;

  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  void SetInput(uint32_t id, float x, float z, bool run);

  void Step(std::chrono::nanoseconds time);

  const std::map<uint32_t, Player>& players() const { return players_; }

 private:
  std::map<uint32_t, Player> players_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WORLD_H_
