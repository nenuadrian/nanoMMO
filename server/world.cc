#include "server/world.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace nanommo {

void World::Join(uint32_t id, Player player) {
  players_[id] = std::move(player);
}

void World::Leave(uint32_t id) { players_.erase(id); }

void World::SetInput(uint32_t id, float x, float z, bool run) {
  const auto found = players_.find(id);
  if (found == players_.end() || !std::isfinite(x) || !std::isfinite(z)) {
    return;
  }
  // So that moving diagonally isn't faster, and a modified client can't ask
  // for more speed.
  const float length = std::hypot(x, z);
  if (length > 1.0f) {
    x /= length;
    z /= length;
  }
  found->second.input_x = x;
  found->second.input_z = z;
  found->second.input_run = run;
}

void World::Step(std::chrono::nanoseconds time) {
  const float seconds = std::chrono::duration<float>(time).count();
  for (auto& [id, player] : players_) {
    const float speed = player.input_run ? kRunSpeed : kWalkSpeed;
    const float x =
        std::clamp(player.x + player.input_x * speed * seconds, -kEdge, kEdge);
    const float z =
        std::clamp(player.z + player.input_z * speed * seconds, -kEdge, kEdge);
    // Motion comes from how far it actually went, so that a player pushing
    // against the edge stands still, instead of walking on the spot.
    const float dx = x - player.x;
    const float dz = z - player.z;
    if (dx == 0.0f && dz == 0.0f) {
      player.motion = Motion::kIdle;
    } else {
      player.motion = player.input_run ? Motion::kRun : Motion::kWalk;
      player.yaw = std::atan2(dx, dz);
    }
    player.x = x;
    player.z = z;
  }
}

}  // namespace nanommo
