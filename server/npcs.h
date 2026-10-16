#ifndef NANOMMO_SERVER_NPCS_H_
#define NANOMMO_SERVER_NPCS_H_

#include "content/npcs.h"
#include "server/world.h"

namespace nanommo {

/// Spawns the land's NPCs from the table, where the ground is walkable. The
/// index is the land's place in kLands.
void SpawnNpcs(World& land, int index, const NpcTable& table);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_NPCS_H_
