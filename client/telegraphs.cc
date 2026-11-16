#include "client/telegraphs.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace nanommo {

float Telegraph::Filled() const {
  const float shows = static_cast<float>(skill.telegraph_ms()) / 1000.0f;
  return shows > 0.0f ? std::clamp(1.0f - left / shows, 0.0f, 1.0f) : 1.0f;
}

void Telegraphs::Update(const protocol::World& world, float seconds) {
  std::map<uint32_t, Telegraph> shown;
  for (const protocol::Monster& monster : world.monsters()) {
    const auto kind = kinds_.find(monster.kind());
    if (monster.motion() != protocol::MOTION_SKILL || kind == kinds_.end()) {
      continue;
    }
    const auto skill = std::ranges::find(kind->second.skills(), monster.skill(),
                                         &monsters::Skill::id);
    if (skill == kind->second.skills().end()) {
      continue;
    }
    const float left = static_cast<float>(monster.lands_in()) / 1000.0f;
    const auto before = shown_.find(monster.id());
    if (before == shown_.end() || before->second.skill.id() != skill->id() ||
        left > before->second.left + kLate) {
      shown[monster.id()] = {.skill = *skill,
                             .at = {.x = monster.x(), .z = monster.z()},
                             .yaw = monster.yaw(),
                             .left = left};
      continue;
    }
    // A world comes twenty times a second, and we draw more often, so it
    // counts down by itself in between. A world that came late doesn't wind
    // it back.
    Telegraph telegraph = before->second;
    telegraph.left = std::max(std::min(telegraph.left - seconds, left), 0.0f);
    shown[monster.id()] = std::move(telegraph);
  }
  shown_ = std::move(shown);
}

}  // namespace nanommo
