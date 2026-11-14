#include "server/walking.h"

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

TEST(Walking, StopsAtTheEdge) {
  const nanommo::Terrain flat;
  float x = flat.edge() - 0.1f;
  float z = 0.0f;
  nanommo::WalkAlong(flat, x, z, 1.0f, 0.0f, true, 1.0f);
  EXPECT_FLOAT_EQ(x, flat.edge());
}
