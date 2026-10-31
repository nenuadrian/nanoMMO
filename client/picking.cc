#include "client/picking.h"

#include <raymath.h>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "content/npcs.h"

namespace nanommo {
namespace {

// A box around a person standing there, in metres: as wide as their arms
// hang, and as tall as them.
constexpr float kHalfWidth = 0.4f;
constexpr float kHeight = 1.8f;

// For the ground: half a metre at a time, up to as far as the fog lets anyone
// see.
constexpr float kStep = 0.5f;
constexpr float kFarthest = 150.0f;

// The nearest of the figures along the ray that the test lets through.
template <typename Test>
const Figure* Pick(const std::vector<Figure>& figures, const Ray& ray,
                   Test test) {
  const Figure* picked = nullptr;
  float nearest = std::numeric_limits<float>::infinity();
  for (const Figure& figure : figures) {
    if (!test(figure)) {
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

}  // namespace

const Figure* PickNpc(const std::vector<Figure>& figures, const Ray& ray,
                      Vector3 you) {
  return Pick(figures, ray, [you](const Figure& figure) {
    return figure.npc &&
           (figure.talks || !figure.teaches.empty() ||
            !figure.quests.empty()) &&
           std::hypot(figure.position.x - you.x, figure.position.z - you.z) <=
               kTalkReach;
  });
}

const Figure* PickMonster(const std::vector<Figure>& figures, const Ray& ray) {
  return Pick(figures, ray, [](const Figure& figure) {
    return figure.monster && figure.to != protocol::MOTION_DEAD;
  });
}

std::optional<Vector3> PickGround(const Terrain& terrain, const Ray& ray) {
  const auto below = [&terrain, &ray](float along) {
    const Vector3 at =
        Vector3Add(ray.position, Vector3Scale(ray.direction, along));
    return at.y < terrain.Height(at.x, at.z);
  };
  // In steps along the ray, until one's under the ground, then halving the
  // last step, back and forth, to where it went under.
  float near = 0.0f;
  for (float far = kStep; far <= kFarthest; far += kStep) {
    if (below(far)) {
      for (int halving = 0; halving < 12; ++halving) {
        const float middle = (near + far) / 2.0f;
        (below(middle) ? far : near) = middle;
      }
      return Vector3Add(ray.position, Vector3Scale(ray.direction, far));
    }
    near = far;
  }
  return std::nullopt;
}

}  // namespace nanommo
