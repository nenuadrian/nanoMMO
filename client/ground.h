#ifndef NANOMMO_CLIENT_GROUND_H_
#define NANOMMO_CLIENT_GROUND_H_

#include <raylib.h>

#include "client/triangles.h"
#include "terrain/terrain.h"

namespace nanommo {

/// The two triangles in each cell of a square of cells, from the cell at
/// left and top, coloured by height and slope.
void AddGround(Triangles& triangles, const Terrain& terrain, int left, int top,
               int cells);

/// A square at sea level, four times as wide as the terrain, so that the sea
/// goes on past it.
Mesh SeaMesh(const Terrain& terrain);

constexpr Color kSea{48, 118, 178, 210};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GROUND_H_
