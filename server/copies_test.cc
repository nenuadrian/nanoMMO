#include "server/copies.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "gtest/gtest.h"
#include "server/world.h"
#include "terrain/lands.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

// By their places in kLands.
constexpr int kMeadows = 0;
constexpr int kAshlands = 1;
constexpr int kCrypt = 2;

// The first gate that leads to the land, which for the others is the
// Meadows' one.
nanommo::Place GateTo(int land) {
  return std::ranges::find(nanommo::kGates, land, &nanommo::Gate::to)->place;
}

// The id of the copy the player's in.
uint32_t CopyOf(const nanommo::Copies& copies, uint32_t id) {
  for (const auto& [copy_id, copy] : copies.all()) {
    if (copy.world.players().contains(id)) {
      return copy_id;
    }
  }
  return 0;
}

}  // namespace

TEST(Copies, StartWithACopyOfEachOpenLand) {
  const nanommo::Copies copies(1, {});

  ASSERT_EQ(copies.all().size(), 2);
  for (const auto& [id, copy] : copies.all()) {
    EXPECT_FALSE(nanommo::kLands[copy.land].dungeon);
    EXPECT_EQ(copy.owner, 0);
  }
}

TEST(Copies, ShareAnOpenLand) {
  nanommo::Copies copies(1, {});
  copies.Join(7, {.name = "alice", .land = kMeadows});
  copies.Join(8, {.name = "bob", .land = kMeadows});

  EXPECT_EQ(CopyOf(copies, 7), CopyOf(copies, 8));
  EXPECT_EQ(copies.Of(7)->land, kMeadows);
}

TEST(Copies, TakeAPlayerThroughTheGate) {
  nanommo::Copies copies(1, {});
  const nanommo::Place gate = GateTo(kAshlands);
  copies.Join(7, {.name = "alice", .x = gate.x, .z = gate.z, .land = kMeadows});
  copies.Step(kTick);

  ASSERT_NE(copies.Of(7), nullptr);
  EXPECT_EQ(copies.Of(7)->land, kAshlands);
  EXPECT_EQ(copies.Of(7)->world.players().at(7).land, kAshlands);
  EXPECT_EQ(copies.players(), 1);
}

TEST(Copies, GiveEachPlayerADungeonOfTheirOwn) {
  nanommo::Copies copies(1, {});
  const nanommo::Place gate = GateTo(kCrypt);
  copies.Join(7, {.name = "alice", .x = gate.x, .z = gate.z, .land = kMeadows});
  copies.Join(8, {.name = "bob", .x = gate.x, .z = gate.z, .land = kMeadows});
  copies.Step(kTick);

  ASSERT_NE(copies.Of(7), nullptr);
  ASSERT_NE(copies.Of(8), nullptr);
  EXPECT_EQ(copies.Of(7)->land, kCrypt);
  EXPECT_EQ(copies.Of(7)->owner, 7);
  EXPECT_EQ(copies.Of(8)->owner, 8);
  EXPECT_NE(CopyOf(copies, 7), CopyOf(copies, 8));
  EXPECT_EQ(copies.all().size(), 4);
  EXPECT_EQ(copies.players(), 2);
}
