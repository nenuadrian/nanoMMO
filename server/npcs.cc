#include "server/npcs.h"

#include <iostream>

namespace nanommo {

void SpawnNpcs(World& land, int index) {
  for (const NpcSpawn& spawn : kNpcSpawns) {
    if (spawn.land != index) {
      continue;
    }
    const NpcKind& kind = *spawn.kind;
    // A seed of its own can put the sea or a tree where the table says.
    if (!land.terrain().Walkable(spawn.x, spawn.z)) {
      std::cerr << "no " << kind.name << " at " << spawn.x << ", " << spawn.z
                << ": the ground isn't walkable there\n";
      continue;
    }
    land.Spawn(Npc{.name = kind.name,
                   .x = spawn.x,
                   .z = spawn.z,
                   .yaw = spawn.yaw,
                   .color = kind.color,
                   .radius = kind.radius,
                   .talks = kind.talks});
  }
}

}  // namespace nanommo
