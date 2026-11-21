#include "server/walking.h"

#include <algorithm>
#include <cmath>

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
  // A diagonal goes no faster than a straight line. The server's input is
  // that long at most already, but the client walks you with the keys as they
  // are, and W and D together would take it ahead of the server.
  const float length = std::hypot(direction_x, direction_z);
  if (length > 1.0f) {
    direction_x /= length;
    direction_z /= length;
  }
  const float speed = run ? kRunSpeed : kWalkSpeed;
  const float edge = terrain.edge();
  Walk(terrain, x, z,
       std::clamp(x + direction_x * speed * seconds, -edge, edge),
       std::clamp(z + direction_z * speed * seconds, -edge, edge));
}

}  // namespace nanommo
