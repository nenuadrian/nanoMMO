#include "client/reactions.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace nanommo {

void Reactions::Hit(uint32_t monster, uint32_t player) {
  blows_[monster] = Blow{.player = player};
}

void Reactions::Update(float seconds) {
  for (auto& [id, blow] : blows_) {
    blow.age += seconds;
  }
  // By then a monster's done rocking.
  std::erase_if(blows_,
                [](const auto& entry) { return entry.second.age > kHeld; });
}

void Reactions::Apply(std::vector<Figure>& figures) {
  std::map<uint32_t, const Figure*> players;
  for (const Figure& figure : figures) {
    if (!figure.monster && !figure.npc) {
      players[figure.id] = &figure;
    }
  }
  for (Figure& figure : figures) {
    if (!figure.monster) {
      continue;
    }
    const auto found = blows_.find(figure.id);
    if (found != blows_.end()) {
      Blow& blow = found->second;
      const auto player = players.find(blow.player);
      if (player != players.end()) {
        const Vector3 from = player->second->position;
        blow.away =
            std::atan2(figure.position.x - from.x, figure.position.z - from.z);
        blow.aimed = true;
      }
      // Fast back, as the blow lands, then slowly upright again.
      const float t = blow.age;
      const float rock =
          t < kRockIn ? t / kRockIn : std::exp(-(t - kRockIn) / kRockOut);
      figure.flash = std::max(1.0f - t / kFlashTime, 0.0f);
      figure.lean = blow.aimed ? kRock * rock : 0.0f;
      figure.lean_yaw = blow.away;
    }
  }
}

}  // namespace nanommo
