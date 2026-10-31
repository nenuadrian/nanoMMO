#include "client/cube.h"

#include <raylib.h>
#include <raymath.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace nanommo {

namespace {

// Around the edge of a disc.
constexpr int kSides = 24;

// Where a spire's sides stop and its point starts, and how wide it is there,
// from the middle, with the bottom half a unit wide.
constexpr float kShoulder = 0.2f;
constexpr float kNarrow = 0.38f;

// Both shapes are convex, so a face points out when it points away from the
// middle: a triangle that doesn't is turned over.
void AddOutward(Triangles& triangles, Vector3 a, Vector3 b, Vector3 c,
                Vector3 middle) {
  const Vector3 centre =
      Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.0f / 3);
  if (Vector3DotProduct(FaceNormal(a, b, c), Vector3Subtract(centre, middle)) <
      0.0f) {
    std::swap(b, c);
  }
  triangles.Add(a, b, c, WHITE);
}

}  // namespace

Triangles Disc() {
  Triangles disc;
  const Vector3 middle{0.0f, 0.0f, 0.0f};
  const auto rim = [](int side, float y) {
    const float angle = 2.0f * std::numbers::pi_v<float> * side / kSides;
    return Vector3{0.5f * std::cos(angle), y, 0.5f * std::sin(angle)};
  };
  for (int side = 0; side < kSides; ++side) {
    const Vector3 top = rim(side, 0.5f);
    const Vector3 next_top = rim(side + 1, 0.5f);
    const Vector3 bottom = rim(side, -0.5f);
    const Vector3 next_bottom = rim(side + 1, -0.5f);
    // From the top's middle, so that it points up, not out.
    AddOutward(disc, {0.0f, 0.5f, 0.0f}, top, next_top, middle);
    AddOutward(disc, bottom, next_bottom, next_top, middle);
    AddOutward(disc, bottom, next_top, top, middle);
  }
  return disc;
}

Triangles Spire() {
  Triangles spire;
  const Vector3 middle{0.0f, 0.0f, 0.0f};
  // Its corners, around the vertical, at the bottom and at the shoulder.
  constexpr std::array<Vector2, 4> kCorners = {
      {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
  const Vector3 point{0.0f, 0.5f, 0.0f};
  for (std::size_t i = 0; i < kCorners.size(); ++i) {
    const Vector2 here = kCorners[i];
    const Vector2 next = kCorners[(i + 1) % kCorners.size()];
    const Vector3 low{0.5f * here.x, -0.5f, 0.5f * here.y};
    const Vector3 next_low{0.5f * next.x, -0.5f, 0.5f * next.y};
    const Vector3 high{kNarrow * here.x, kShoulder, kNarrow * here.y};
    const Vector3 next_high{kNarrow * next.x, kShoulder, kNarrow * next.y};
    AddOutward(spire, low, next_low, next_high, middle);
    AddOutward(spire, low, next_high, high, middle);
    AddOutward(spire, high, next_high, point, middle);
  }
  return spire;
}

}  // namespace nanommo
