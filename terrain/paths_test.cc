#include "terrain/paths.h"

#include <cmath>
#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace {

// Each leg of the way is a straight walk over open squares, from where the
// last one ended.
void ExpectWalkable(const nanommo::Paths& paths, nanommo::Place from,
                    const std::vector<nanommo::Place>& way) {
  nanommo::Place at = from;
  for (const nanommo::Place& next : way) {
    EXPECT_TRUE(paths.Clear(at, next))
        << "(" << at.x << ", " << at.z << ") to (" << next.x << ", " << next.z
        << ")";
    at = next;
  }
}

}  // namespace

TEST(Paths, WalksStraightWithNothingInTheWay) {
  nanommo::Paths paths{nanommo::Terrain()};
  const std::vector<nanommo::Place> way = paths.Find({-4, -4}, {4, 3});
  ASSERT_EQ(way.size(), 1u);
  EXPECT_EQ(way[0].x, 4.0f);
  EXPECT_EQ(way[0].z, 3.0f);
  EXPECT_EQ(paths.searched(), 0);
}

TEST(Paths, GoesNowhereItCantStand) {
  const nanommo::Terrain island(1);
  nanommo::Paths paths(island);
  // Out at sea, past the island.
  EXPECT_TRUE(paths.Find({0, 0}, {250, 250}).empty());
  EXPECT_TRUE(paths.Find({0, 0}, {900, 0}).empty());
}

TEST(Paths, GoesAroundATree) {
  const nanommo::Terrain island(1);
  nanommo::Paths paths(island);
  // A tree with open ground two metres either side of it.
  std::optional<nanommo::Tree> found;
  for (const nanommo::Tree& tree : island.trees()) {
    if (paths.Open({tree.x - 2, tree.z}) && paths.Open({tree.x + 2, tree.z}) &&
        paths.Open({tree.x, tree.z - 2}) && paths.Open({tree.x, tree.z + 2})) {
      found = tree;
      break;
    }
  }
  ASSERT_TRUE(found);
  const nanommo::Place from{found->x - 2, found->z};
  const nanommo::Place to{found->x + 2, found->z};
  ASSERT_FALSE(paths.Clear(from, to));

  const std::vector<nanommo::Place> way = paths.Find(from, to);
  ASSERT_GE(way.size(), 2u);
  ExpectWalkable(paths, from, way);
  EXPECT_EQ(way.back().x, to.x);
  EXPECT_EQ(way.back().z, to.z);
  EXPECT_GT(paths.searched(), 0);
}

TEST(Paths, CrossesTheIslandOrGetsAsNearAsItCan) {
  const nanommo::Terrain island(1);
  nanommo::Paths paths(island);
  // From the middle to open ground far off in each direction.
  for (const nanommo::Place far :
       {nanommo::Place{-150, -150}, nanommo::Place{150, -150},
        nanommo::Place{-150, 150}, nanommo::Place{150, 150}}) {
    nanommo::Place to = far;
    while (!paths.Open(to)) {
      to = {to.x * 0.9f, to.z * 0.9f};
    }
    const std::vector<nanommo::Place> way = paths.Find({0.5f, 0.5f}, to);
    ASSERT_FALSE(way.empty());
    ExpectWalkable(paths, {0.5f, 0.5f}, way);
    EXPECT_EQ(way.back().x, to.x);
    EXPECT_EQ(way.back().z, to.z);
  }
}

TEST(Paths, PutsGroundUnderEveryStep) {
  const nanommo::Terrain island(1);
  nanommo::Paths paths(island);
  // Ways between places all over the island, each leg walked five centimetres
  // at a time: every step is on ground a player can stand on, as the server
  // gives up on a walk at the first step that isn't.
  const float edge = island.half_size() * 0.8f;
  int ways = 0;
  for (float from_x = -edge; from_x < edge; from_x += 37.0f) {
    for (float to_z = -edge; to_z < edge; to_z += 41.0f) {
      const nanommo::Place from{from_x, -to_z * 0.7f};
      const nanommo::Place to{-from_x * 0.6f + 13.0f, to_z};
      if (!paths.Open(from) || !island.Walkable(to.x, to.z)) {
        continue;
      }
      const std::vector<nanommo::Place> way = paths.Find(from, to);
      nanommo::Place at = from;
      for (const nanommo::Place& next : way) {
        const float length = std::hypot(next.x - at.x, next.z - at.z);
        for (float walked = 0.05f; walked < length; walked += 0.05f) {
          const float share = walked / length;
          const float x = at.x + (next.x - at.x) * share;
          const float z = at.z + (next.z - at.z) * share;
          ASSERT_TRUE(island.Walkable(x, z))
              << "(" << x << ", " << z << ") on the way from (" << from.x
              << ", " << from.z << ") to (" << to.x << ", " << to.z << ")";
        }
        at = next;
      }
      ways += way.empty() ? 0 : 1;
    }
  }
  EXPECT_GT(ways, 50);
}
