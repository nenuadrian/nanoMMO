#ifndef NANOMMO_SERVER_WORLD_H_
#define NANOMMO_SERVER_WORLD_H_

#include <chrono>
#include <cstdint>
#include <map>

namespace nanommo {

struct Player {
  float x = 0.0f;
  float z = 0.0f;
  float input_x = 0.0f;
  float input_z = 0.0f;
};

class World {
 public:
  /// In tiles per second.
  static constexpr float kSpeed = 4.0f;

  /// Six tiles to the ground's edge, less half a tile, so players stay on it.
  static constexpr float kEdge = 5.5f;

  uint32_t Join();

  void Leave(uint32_t id);

  void SetInput(uint32_t id, float x, float z);

  void Step(std::chrono::nanoseconds time);

  const std::map<uint32_t, Player>& players() const { return players_; }

 private:
  uint32_t next_id_ = 1;
  std::map<uint32_t, Player> players_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WORLD_H_
