#include "terrain/lands.h"

#include <algorithm>
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

bool InGate(const Gate& gate, float x, float z) {
  return std::hypot(x - gate.place.x, z - gate.place.z) < kGateRadius;
}

const Gate& Twin(const Gate& gate) {
  // GatesLeadBack makes sure there's one.
  return *std::ranges::find_if(kGates, [&gate](const Gate& other) {
    return other.land == gate.to && other.to == gate.land;
  });
}

Place Arrival(const Gate& gate, float yaw) {
  const Place& twin = Twin(gate).place;
  return {.x = twin.x + kArrival * std::sin(yaw),
          .z = twin.z + kArrival * std::cos(yaw)};
}

}  // namespace nanommo
