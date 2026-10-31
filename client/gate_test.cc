#include "client/gate.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "client/cube.h"
#include "gtest/gtest.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

TEST(Gate, IsARoundPlatformInARingOfPointedStonesOnBases) {
  const nanommo::Terrain board;
  // The Meadows have two gates.
  const std::vector<nanommo::Cube> cubes = nanommo::GateCubes(board, 0);
  const auto count = [&cubes](nanommo::Shape shape) {
    return std::ranges::count(cubes, shape, &nanommo::Cube::shape);
  };
  EXPECT_EQ(count(nanommo::Shape::kSpire), 12);
  EXPECT_EQ(count(nanommo::Shape::kDisc), 4);
  EXPECT_EQ(count(nanommo::Shape::kBox), 12);

  // The sky's colour sits inside the rim, and stands a little above it.
  std::vector<nanommo::Cube> discs;
  std::ranges::copy_if(cubes, std::back_inserter(discs),
                       [](const nanommo::Cube& cube) {
                         return cube.shape == nanommo::Shape::kDisc;
                       });
  const nanommo::Cube& rim = discs[0];
  const nanommo::Cube& inside = discs[1];
  EXPECT_LT(inside.size.x, rim.size.x);
  EXPECT_GT(inside.position.y + inside.size.y / 2,
            rim.position.y + rim.size.y / 2);
  EXPECT_FLOAT_EQ(inside.size.x, 2 * nanommo::kGateRadius);
}
