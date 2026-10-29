#include "client/picking.h"

#include <limits>
#include <vector>

namespace nanommo {
namespace {

// A box around a person standing there, in metres: as wide as their arms
// hang, and as tall as them.
constexpr float kHalfWidth = 0.4f;
constexpr float kHeight = 1.8f;

}  // namespace

const Figure* PickNpc(const std::vector<Figure>& figures, const Ray& ray) {
  const Figure* picked = nullptr;
  float nearest = std::numeric_limits<float>::infinity();
  for (const Figure& figure : figures) {
    if (!figure.npc ||
        (!figure.talks && figure.teaches.empty() && figure.quests.empty())) {
      continue;
    }
    const Vector3& at = figure.position;
    const RayCollision hit = GetRayCollisionBox(
        ray, {.min = {at.x - kHalfWidth, at.y, at.z - kHalfWidth},
              .max = {at.x + kHalfWidth, at.y + kHeight, at.z + kHalfWidth}});
    if (hit.hit && hit.distance < nearest) {
      nearest = hit.distance;
      picked = &figure;
    }
  }
  return picked;
}

}  // namespace nanommo
