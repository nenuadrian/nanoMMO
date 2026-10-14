#include "terrain/lands.h"

#include <cmath>

namespace nanommo {

bool InGate(const Land& land, float x, float z) {
  return std::hypot(x - land.gate.x, z - land.gate.z) < kGateRadius;
}

}  // namespace nanommo
