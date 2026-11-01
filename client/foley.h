#ifndef NANOMMO_CLIENT_FOLEY_H_
#define NANOMMO_CLIENT_FOLEY_H_

#include <array>
#include <cstdint>

#include "client/synth.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What a foot comes down on, which is how the step sounds.
enum class Ground { kGrass, kSand, kRock, kSnow, kAsh, kStone, kWater };

constexpr std::array<Ground, 7> kGrounds = {
    Ground::kGrass, Ground::kSand,  Ground::kRock, Ground::kSnow,
    Ground::kAsh,   Ground::kStone, Ground::kWater};

/// How a land sounds: what each of its surfaces is underfoot.
struct LandSound {
  Ground sand;
  Ground grass;
  Ground rock;
  Ground snow;
};

/// In the order of the lands: the Ashlands are ash where they aren't rock,
/// and the Crypt is stone all over.
constexpr std::array<LandSound, 3> kLandSounds = {{
    {.sand = Ground::kSand,
     .grass = Ground::kGrass,
     .rock = Ground::kRock,
     .snow = Ground::kSnow},
    {.sand = Ground::kAsh,
     .grass = Ground::kAsh,
     .rock = Ground::kRock,
     .snow = Ground::kAsh},
    {.sand = Ground::kStone,
     .grass = Ground::kStone,
     .rock = Ground::kStone,
     .snow = Ground::kStone},
}};
static_assert(kLandSounds.size() == kLands.size(), "each land needs its sound");

/// What's underfoot there: the water we wade in, or the land's surface.
Ground GroundAt(const Terrain& terrain, int land, float x, float z);

/// One footstep: the heel, then the rest of the foot. Each seed makes one a
/// little different from the others.
Samples Step(Ground ground, uint32_t seed);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FOLEY_H_
