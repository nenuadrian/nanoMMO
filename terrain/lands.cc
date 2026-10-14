#include "terrain/lands.h"

#include <cmath>

namespace nanommo {

bool InGate(const Land& land, float x, float z) {
  return std::hypot(x - land.gate.x, z - land.gate.z) < kGateRadius;
}

Place Arrival(const Land& land, float yaw) {
  return {.x = land.gate.x + kArrival * std::sin(yaw),
          .z = land.gate.z + kArrival * std::cos(yaw)};
}

}  // namespace nanommo
