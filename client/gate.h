#ifndef NANOMMO_CLIENT_GATE_H_
#define NANOMMO_CLIENT_GATE_H_

#include <vector>

#include "client/cube.h"
#include "terrain/terrain.h"

namespace nanommo {

/// A ring of standing stones around each of the land's gates, and a slab in
/// the middle in the colour of the sky where it takes you.
std::vector<Cube> GateCubes(const Terrain& terrain, int land);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GATE_H_
