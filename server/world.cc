#include "server/world.h"

#include <algorithm>
#include <cmath>

namespace nanommo {

uint32_t World::Join() {
  const uint32_t id = next_id_++;
  Player player;
  player.x = static_cast<float>((id - 1) % 4) * 2.0f - 3.0f;
  players_[id] = player;
  return id;
}

void World::Leave(uint32_t id) { players_.erase(id); }

void World::SetInput(uint32_t id, float x, float z) {
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
}

void World::Step(std::chrono::nanoseconds time) {
  const float seconds = std::chrono::duration<float>(time).count();
  for (auto& [id, player] : players_) {
    player.x =
        std::clamp(player.x + player.input_x * kSpeed * seconds, -kEdge, kEdge);
    player.z =
        std::clamp(player.z + player.input_z * kSpeed * seconds, -kEdge, kEdge);
  }
}

}  // namespace nanommo
