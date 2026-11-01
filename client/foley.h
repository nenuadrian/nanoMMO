#ifndef NANOMMO_CLIENT_FOLEY_H_
#define NANOMMO_CLIENT_FOLEY_H_

#include <array>
#include <cstdint>

#include "client/synth.h"

namespace nanommo {

/// What a foot comes down on, which is how the step sounds.
enum class Ground { kGrass, kSand, kRock, kSnow, kAsh, kStone, kWater };

constexpr std::array<Ground, 7> kGrounds = {
    Ground::kGrass, Ground::kSand,  Ground::kRock, Ground::kSnow,
    Ground::kAsh,   Ground::kStone, Ground::kWater};

/// One footstep: the heel, then the rest of the foot. Each seed makes one a
/// little different from the others.
Samples Step(Ground ground, uint32_t seed);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FOLEY_H_
