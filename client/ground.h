#ifndef NANOMMO_CLIENT_GROUND_H_
#define NANOMMO_CLIENT_GROUND_H_

#include <raylib.h>

#include "client/atmosphere.h"
#include "client/triangles.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What the ground is at the height, where the y of the ground's normal is up:
/// sand by the sea, snow up high, rock where it's high or steep, and grass
/// elsewhere.
enum class Surface { kSand, kGrass, kRock, kSnow };
Surface SurfaceAt(float height, float up);

/// The surface's colour in the look.
Color GroundColor(const Look& look, float height, float up);

/// The two triangles in each cell of a square of cells, from the cell at
/// left and top, in the look's colours, by height and slope.
void AddGround(Triangles& triangles, const Terrain& terrain, const Look& look,
               int left, int top, int cells);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GROUND_H_
