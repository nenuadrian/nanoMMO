#include "terrain/terrain.h"

#include <bit>
#include <cstdint>

#include "gtest/gtest.h"
#include "terrain/lands.h"

namespace {

// FNV-1a, over the bits of every corner's height, and of where every tree
// stands.
uint32_t Fingerprint(const nanommo::Terrain& terrain) {
  uint32_t hash = 2166136261u;
  const auto add = [&hash](float value) {
    hash ^= std::bit_cast<uint32_t>(value);
    hash *= 16777619u;
  };
  for (int j = 0; j <= terrain.cells(); ++j) {
    for (int i = 0; i <= terrain.cells(); ++i) {
      add(terrain.Corner(i, j));
    }
  }
  for (const nanommo::Tree& tree : terrain.trees()) {
    add(tree.x);
    add(tree.z);
  }
  return hash;
}

}  // namespace

TEST(Terrain, IsTheSameIslandOnEveryMachine) {
  const nanommo::Terrain island(1);

  EXPECT_EQ(Fingerprint(island), 2157307642u);
  EXPECT_NE(Fingerprint(nanommo::Terrain(2)), Fingerprint(island));
}

TEST(Terrain, HasLandInTheMiddleAndSeaAtTheEdge) {
  const nanommo::Terrain island(1);
  const float edge = island.half_size();

  EXPECT_GT(island.Height(0.0f, 0.0f), nanommo::Terrain::kSeaLevel);
  EXPECT_LT(island.Height(edge, 0.0f), nanommo::Terrain::kSeaLevel);
  EXPECT_LT(island.Height(-edge, -edge), nanommo::Terrain::kSeaLevel);
  EXPECT_TRUE(island.Walkable(0.0f, 0.0f));
  EXPECT_FALSE(island.Walkable(island.edge(), 0.0f));
  EXPECT_FALSE(island.Walkable(edge + 1.0f, 0.0f));
  ASSERT_FALSE(island.trees().empty());
  const nanommo::Tree& tree = island.trees().front();
  EXPECT_FALSE(island.Walkable(tree.x, tree.z));
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

TEST(Terrain, HasRoomAtEachGate) {
  for (const uint32_t seed : {1, 2, 3}) {
    for (const nanommo::Land& land : nanommo::kLands) {
      const nanommo::Terrain island(seed + land.seed, land.wading);
      EXPECT_TRUE(island.Walkable(land.gate.x, land.gate.z));
      // Whichever way a player comes through, they land on the island, out of
      // the gate.
      for (float yaw = 0.0f; yaw < 6.3f; yaw += 0.5f) {
        const nanommo::Place arrival = nanommo::Arrival(land, yaw);
        EXPECT_TRUE(island.Walkable(arrival.x, arrival.z));
        EXPECT_FALSE(nanommo::InGate(land, arrival.x, arrival.z));
      }
    }
  }
}
