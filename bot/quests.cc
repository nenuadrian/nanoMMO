#include "bot/quests.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace nanommo {

namespace {

/// The NPC here who gives the quest, and takes it back, if there is one.
const protocol::Npc* Giver(const quests::Quest& quest,
                           const protocol::World& seen) {
  const auto found =
      std::ranges::find(seen.npcs(), quest.giver(), &protocol::Npc::kind);
  return found != seen.npcs().end() ? &*found : nullptr;
}

Errand AtGiver(Errand::Kind kind, const protocol::Npc& npc, uint32_t quest) {
  Errand errand{.kind = kind, .x = npc.x(), .z = npc.z()};
  errand.quest.set_npc(npc.id());
  errand.quest.set_quest(quest);
  return errand;
}

}  // namespace

std::optional<Errand> NextErrand(const Content& content,
                                 const protocol::Quests& quests,
                                 const protocol::World& seen, float x,
                                 float z) {
  std::optional<Errand> hunt;
  float nearest = std::numeric_limits<float>::infinity();
  for (const auto& [id, killed] : quests.on()) {
    const auto quest = content.quests.find(id);
    if (quest == content.quests.end()) {
      continue;
    }
    if (killed >= quest->second.kill().count()) {
      if (const protocol::Npc* giver = Giver(quest->second, seen)) {
        return AtGiver(Errand::Kind::kTurnIn, *giver, id);
      }
      continue;
    }
    for (const protocol::Monster& monster : seen.monsters()) {
      const float distance = std::hypot(monster.x() - x, monster.z() - z);
      if (monster.kind() == quest->second.kill().monster() &&
          monster.motion() != protocol::MOTION_DEAD && distance < nearest) {
        nearest = distance;
        hunt = Errand{
            .kind = Errand::Kind::kHunt, .x = monster.x(), .z = monster.z()};
      }
    }
  }
  if (hunt) {
    return hunt;
  }
  for (const auto& [id, quest] : content.quests) {
    if (quests.on().contains(id) ||
        std::ranges::find(quests.done(), id) != quests.done().end()) {
      continue;
    }
    if (const protocol::Npc* giver = Giver(quest, seen)) {
      return AtGiver(Errand::Kind::kTake, *giver, id);
    }
  }
  return std::nullopt;
}

}  // namespace nanommo
