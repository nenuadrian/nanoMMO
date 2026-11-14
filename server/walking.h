#ifndef NANOMMO_SERVER_WALKING_H_
#define NANOMMO_SERVER_WALKING_H_

#include "terrain/terrain.h"

namespace nanommo {

/// In tiles a second. A tile is a metre, and walking is the walk clip's speed.
inline constexpr float kWalkSpeed = 1.8f;
inline constexpr float kRunSpeed = 3.1f;

/// Moves to the place, or if the terrain's in the way there, as far as it can
/// along x or along z alone, so that it slides along a coast.
void Walk(const Terrain& terrain, float& x, float& z, float to_x, float to_z);

/// Walks from x, z for that many seconds, along the direction, at the walk's
/// speed or the run's, inside the land's edge and on land. The server walks a
/// player with it each tick, and the client walks you with it before the
/// server answers, so both have to come out the same.
void WalkAlong(const Terrain& terrain, float& x, float& z, float direction_x,
               float direction_z, bool run, float seconds);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WALKING_H_
