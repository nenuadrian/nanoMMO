#include "server/walking.h"

#include <algorithm>

namespace nanommo {

void Walk(const Terrain& terrain, float& x, float& z, float to_x, float to_z) {
  if (terrain.Walkable(to_x, to_z)) {
    x = to_x;
    z = to_z;
  } else if (terrain.Walkable(to_x, z)) {
    x = to_x;
  } else if (terrain.Walkable(x, to_z)) {
    z = to_z;
  }
}

void WalkAlong(const Terrain& terrain, float& x, float& z, float direction_x,
               float direction_z, bool run, float seconds) {
  const float speed = run ? kRunSpeed : kWalkSpeed;
  const float edge = terrain.edge();
  Walk(terrain, x, z,
       std::clamp(x + direction_x * speed * seconds, -edge, edge),
       std::clamp(z + direction_z * speed * seconds, -edge, edge));
}

}  // namespace nanommo
