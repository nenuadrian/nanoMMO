#include "terrain/terrain.h"

#include <bit>
#include <cstdint>

#include "gtest/gtest.h"

namespace {

// FNV-1a, over the bits of every corner's height.
uint32_t Fingerprint(const nanommo::Terrain& terrain) {
  uint32_t hash = 2166136261u;
  for (int j = 0; j <= terrain.cells(); ++j) {
    for (int i = 0; i <= terrain.cells(); ++i) {
      hash ^= std::bit_cast<uint32_t>(terrain.Corner(i, j));
      hash *= 16777619u;
    }
  }
  return hash;
}

}  // namespace

TEST(Terrain, IsTheSameIslandOnEveryMachine) {
  const nanommo::Terrain island(1);

  EXPECT_EQ(Fingerprint(island), 1043257573u);
  EXPECT_NE(Fingerprint(nanommo::Terrain(2)), Fingerprint(island));
}

TEST(Terrain, HasLandInTheMiddleAndSeaAtTheEdge) {
  const nanommo::Terrain island(1);
  const float edge = island.half_size();

  EXPECT_GT(island.Height(0.0f, 0.0f), nanommo::Terrain::kSeaLevel);
  EXPECT_LT(island.Height(edge, 0.0f), nanommo::Terrain::kSeaLevel);
  EXPECT_LT(island.Height(-edge, -edge), nanommo::Terrain::kSeaLevel);
}

TEST(Terrain, StandsOnTheTrianglesBetweenTheCorners) {
  const nanommo::Terrain island(1);
  const float x = 3 * nanommo::Terrain::kCell - island.half_size();
  const float z = 5 * nanommo::Terrain::kCell - island.half_size();
  const float cell = nanommo::Terrain::kCell;

  EXPECT_FLOAT_EQ(island.Height(x, z), island.Corner(3, 5));
  EXPECT_FLOAT_EQ(island.Height(x + cell / 2, z + cell / 2),
                  (island.Corner(3, 5) + island.Corner(4, 6)) / 2);
}
