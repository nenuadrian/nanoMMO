#include "server/walking.h"

#include <cmath>

#include "gtest/gtest.h"
#include "terrain/terrain.h"

TEST(Walking, GoesAlongTheDirectionAtTheSpeed) {
  const nanommo::Terrain flat;
  float x = 0.0f;
  float z = 0.0f;
  nanommo::WalkAlong(flat, x, z, 1.0f, 0.0f, false, 0.5f);
  EXPECT_FLOAT_EQ(x, nanommo::kWalkSpeed * 0.5f);
  nanommo::WalkAlong(flat, x, z, 0.0f, -1.0f, true, 1.0f);
  EXPECT_FLOAT_EQ(z, -nanommo::kRunSpeed);
}

TEST(Walking, GoesNoFasterOnADiagonal) {
  const nanommo::Terrain flat;
  float x = 0.0f;
  float z = 0.0f;
  nanommo::WalkAlong(flat, x, z, 1.0f, 1.0f, true, 1.0f);
  EXPECT_FLOAT_EQ(std::hypot(x, z), nanommo::kRunSpeed);
  EXPECT_FLOAT_EQ(x, z);
  // A direction shorter than one, a stick pushed half way, still goes slower.
  x = 0.0f;
  z = 0.0f;
  nanommo::WalkAlong(flat, x, z, 0.5f, 0.0f, false, 1.0f);
  EXPECT_FLOAT_EQ(x, nanommo::kWalkSpeed * 0.5f);
}

TEST(Walking, StopsAtTheEdge) {
  const nanommo::Terrain flat;
  float x = flat.edge() - 0.1f;
  float z = 0.0f;
  nanommo::WalkAlong(flat, x, z, 1.0f, 0.0f, true, 1.0f);
  EXPECT_FLOAT_EQ(x, flat.edge());
}
