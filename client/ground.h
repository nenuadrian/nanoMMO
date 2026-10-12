#ifndef NANOMMO_CLIENT_GROUND_H_
#define NANOMMO_CLIENT_GROUND_H_

#include <raylib.h>

#include "terrain/terrain.h"

namespace nanommo {

/// The terrain's two triangles in each cell, coloured by height and slope.
Mesh GroundMesh(const Terrain& terrain);

/// A square at sea level, four times as wide as the terrain, so that the sea
/// goes on past it.
Mesh SeaMesh(const Terrain& terrain);

constexpr Color kSea{48, 118, 178, 210};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GROUND_H_
