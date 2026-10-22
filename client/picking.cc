#include "client/picking.h"

#include <limits>
#include <vector>

namespace nanommo {

const Figure* PickNpc(const std::vector<Figure>& figures, const Ray& ray) {
  const Figure* picked = nullptr;
  float nearest = std::numeric_limits<float>::infinity();
  for (const Figure& figure : figures) {
    if (!figure.npc || (!figure.talks && figure.teaches.empty())) {
      continue;
    }
    // The ball rests on the ground, so its middle is a radius up.
    const Vector3 middle{figure.position.x, figure.position.y + figure.radius,
                         figure.position.z};
    const RayCollision hit = GetRayCollisionSphere(ray, middle, figure.radius);
    if (hit.hit && hit.distance < nearest) {
      nearest = hit.distance;
      picked = &figure;
    }
  }
  return picked;
}

}  // namespace nanommo
