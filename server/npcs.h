#ifndef NANOMMO_SERVER_NPCS_H_
#define NANOMMO_SERVER_NPCS_H_

#include <array>
#include <cstdint>

#include "server/world.h"

namespace nanommo {

/// What every NPC of a kind looks like. The server sends the look with each
/// NPC, so the client needs no table of its own.
struct NpcKind {
  const char* name;
  /// As 0xRRGGBBAA.
  uint32_t color = 0xFFFFFFFF;
  /// In metres.
  float radius = 0.5f;
};

constexpr NpcKind kGuard = {
    .name = "Guard",
    .color = 0x3E6FD8FF,
    .radius = 0.6f,
};

constexpr NpcKind kMerchant = {
    .name = "Merchant",
    .color = 0xE3B53BFF,
    .radius = 0.5f,
};

constexpr NpcKind kHealer = {
    .name = "Healer",
    .color = 0x5BC46BFF,
    .radius = 0.4f,
};

constexpr NpcKind kWarden = {
    .name = "Ash Warden",
    .color = 0xD8552EFF,
    .radius = 0.7f,
};

/// Where an NPC stands, in metres from the middle of its land, and which way
/// it faces.
struct NpcSpawn {
  const NpcKind* kind;
  /// By its place in kLands.
  int land = 0;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
};

// Around the middle of each land, where the players start: a guard by the road
// to the gate, facing the players, and a merchant and a healer facing each
// other across it.
constexpr std::array<NpcSpawn, 6> kNpcSpawns = {{
    {.kind = &kGuard, .land = 0, .x = -3.2f, .z = -7.5f, .yaw = 0.0f},
    {.kind = &kMerchant, .land = 0, .x = -6.0f, .z = -4.0f, .yaw = 1.57f},
    {.kind = &kHealer, .land = 0, .x = 6.0f, .z = -4.5f, .yaw = -1.57f},
    {.kind = &kWarden, .land = 1, .x = 3.2f, .z = -7.5f, .yaw = 0.0f},
    {.kind = &kMerchant, .land = 1, .x = -6.0f, .z = -4.0f, .yaw = 1.57f},
    {.kind = &kHealer, .land = 1, .x = 6.0f, .z = -4.5f, .yaw = -1.57f},
}};

/// Spawns the land's NPCs from kNpcSpawns, where the ground is walkable. The
/// index is the land's place in kLands.
void SpawnNpcs(World& land, int index);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_NPCS_H_
