#include "bot/factions.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>

#include "terrain/lands.h"

namespace nanommo {

uint32_t FactionFor(const Content& content, uint32_t seed) {
  if (content.factions.empty()) {
    return 0;
  }
  const auto picked =
      std::next(content.factions.begin(), seed % content.factions.size());
  return picked->first;
}

std::optional<Errand> Oath(const Content& content, uint32_t faction,
                           const protocol::World& seen, int land) {
  const auto found = content.factions.find(faction);
  if (found == content.factions.end()) {
    return std::nullopt;
  }
  const uint32_t kind = found->second.herald();
  const auto herald =
      std::ranges::find(seen.npcs(), kind, &protocol::Npc::kind);
  if (herald != seen.npcs().end()) {
    return Errand{.kind = Errand::Kind::kSwear,
                  .x = herald->x(),
                  .z = herald->z(),
                  .herald = herald->id()};
  }
  // Not in sight yet, as NPCs come once a second, and only those near.
  for (const npcs::Spawn& spawn : content.npcs.spawns) {
    if (spawn.kind() == kind && LandNamed(spawn.land()) == land) {
      return Errand{
          .kind = Errand::Kind::kSwear, .x = spawn.x(), .z = spawn.z()};
    }
  }
  return std::nullopt;
}

bool Sworn(const protocol::World& seen) {
  return std::ranges::any_of(
      seen.players(), [&seen](const protocol::Player& player) {
        return player.id() == seen.you() && player.faction() != 0;
      });
}

}  // namespace nanommo
