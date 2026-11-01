#ifndef NANOMMO_CLIENT_FOLEY_H_
#define NANOMMO_CLIENT_FOLEY_H_

#include <array>
#include <cstdint>

#include "client/synth.h"
#include "content/skills.pb.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What a foot comes down on, which is how the step sounds.
enum class Ground { kGrass, kSand, kRock, kSnow, kAsh, kStone, kWater };

constexpr std::array<Ground, 7> kGrounds = {
    Ground::kGrass, Ground::kSand,  Ground::kRock, Ground::kSnow,
    Ground::kAsh,   Ground::kStone, Ground::kWater};

/// How a land sounds: what each of its surfaces is underfoot, and the chord
/// that greets a player who comes into it, in hertz.
struct LandSound {
  Ground sand;
  Ground grass;
  Ground rock;
  Ground snow;
  std::array<float, 3> chord;
};

/// In the order of the lands: the Meadows in C major, the Ashlands in A
/// minor, and the Crypt on a diminished chord, which doesn't settle.
constexpr std::array<LandSound, 3> kLandSounds = {{
    {.sand = Ground::kSand,
     .grass = Ground::kGrass,
     .rock = Ground::kRock,
     .snow = Ground::kSnow,
     .chord = {523.25f, 659.26f, 783.99f}},
    {.sand = Ground::kAsh,
     .grass = Ground::kAsh,
     .rock = Ground::kRock,
     .snow = Ground::kAsh,
     .chord = {220.0f, 261.63f, 329.63f}},
    {.sand = Ground::kStone,
     .grass = Ground::kStone,
     .rock = Ground::kStone,
     .snow = Ground::kStone,
     .chord = {196.0f, 233.08f, 277.18f}},
}};
static_assert(kLandSounds.size() == kLands.size(), "each land needs its sound");

/// What's underfoot there: the water we wade in, or the land's surface.
Ground GroundAt(const Terrain& terrain, int land, float x, float z);

/// One footstep: the heel, then the rest of the foot. Each seed makes one a
/// little different from the others.
Samples Step(Ground ground, uint32_t seed);

/// How cold the fx's colour is, from -1 for red to 1 for blue: a cold one
/// rings higher, and a warm one lower.
float Coldness(uint32_t color);

/// The sound of one of a skill's fx, from when it starts.
Samples FxSound(const skills::Fx& fx, uint32_t seed);

/// All the sounds of a skill's fx, each at its time: those at the target
/// with a monster picked, as the fx are drawn.
Samples SkillSound(const skills::Look& look, uint32_t seed, bool target);

/// Going through a gate: a rush up, as the screen covers the world.
Samples Departure(uint32_t seed);

/// Coming out into a land: its chord, struck like bells.
Samples Arrival(int land);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FOLEY_H_
