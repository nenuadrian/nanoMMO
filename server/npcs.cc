#include "server/npcs.h"

#include <iostream>

#include "terrain/lands.h"

namespace nanommo {

void SpawnNpcs(World& land, int index, const NpcTable& table) {
  for (const npcs::Spawn& spawn : table.spawns) {
    if (LandNamed(spawn.land()) != index) {
      continue;
    }
    const npcs::Kind& kind = table.kinds.at(spawn.kind());
    // A seed of its own can put the sea or a tree where the table says.
    if (!land.terrain().Walkable(spawn.x(), spawn.z())) {
      std::cerr << "no " << kind.name() << " at " << spawn.x() << ", "
                << spawn.z() << ": the ground isn't walkable there\n";
      continue;
    }
    land.Spawn(Npc{.name = kind.name(),
                   .kind = kind.id(),
                   .x = spawn.x(),
                   .z = spawn.z(),
                   .yaw = spawn.yaw(),
                   .talks = kind.talks(),
                   .teaches = {kind.teaches().begin(), kind.teaches().end()}});
  }
}

}  // namespace nanommo
