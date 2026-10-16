#include "terrain/lands.h"

#include <cmath>
#include <cstddef>
#include <string_view>

namespace nanommo {

int LandNamed(std::string_view name) {
  for (std::size_t i = 0; i < kLands.size(); ++i) {
    if (kLands[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

bool IsLand(std::string_view name) {
  for (const Land& land : kLands) {
    if (land.name == name) {
      return true;
    }
  }
  return false;
}

bool InGate(const Land& land, float x, float z) {
  return std::hypot(x - land.gate.x, z - land.gate.z) < kGateRadius;
}

Place Arrival(const Land& land, float yaw) {
  return {.x = land.gate.x + kArrival * std::sin(yaw),
          .z = land.gate.z + kArrival * std::cos(yaw)};
}

}  // namespace nanommo
