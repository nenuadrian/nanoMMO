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
    for (const nanommo::Gate& gate : nanommo::kGates) {
      const nanommo::Land& land = nanommo::kLands[gate.land];
      const nanommo::Terrain island(seed + land.seed, land.wading);
      EXPECT_TRUE(island.Walkable(gate.place.x, gate.place.z));
      // Whichever way a player comes through the twin, they land on the
      // island, out of each of its gates.
      for (float yaw = 0.0f; yaw < 6.3f; yaw += 0.5f) {
        const nanommo::Place arrival =
            nanommo::Arrival(nanommo::Twin(gate), yaw);
        EXPECT_TRUE(island.Walkable(arrival.x, arrival.z));
        for (const nanommo::Gate& other : nanommo::kGates) {
          if (other.land == gate.land) {
            EXPECT_FALSE(nanommo::InGate(other, arrival.x, arrival.z));
          }
        }
      }
    }
  }
}

TEST(Terrain, ComesOutOfTheGateThatLeadsBack) {
  for (const nanommo::Gate& gate : nanommo::kGates) {
    const nanommo::Gate& twin = nanommo::Twin(gate);
    EXPECT_EQ(twin.land, gate.to);
    EXPECT_EQ(twin.to, gate.land);
    const nanommo::Place arrival = nanommo::Arrival(gate, 0.0f);
    EXPECT_FLOAT_EQ(arrival.x, twin.place.x);
    EXPECT_FLOAT_EQ(arrival.z, twin.place.z + nanommo::kArrival);
  }
}
