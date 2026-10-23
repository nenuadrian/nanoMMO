#include "server/copies.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "content/content.h"
#include "content/monsters.h"
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

void RunFor(nanommo::Copies& copies, std::chrono::seconds time) {
  for (std::chrono::nanoseconds run{0}; run < time; run += kTick) {
    copies.Step(kTick);
  }
}

// A skeleton in the Crypt, away from where players come in.
nanommo::Content CryptWithASkeleton() {
  nanommo::Content content;
  content.monsters = *nanommo::ParseMonsters(R"pb(
    kinds {
      id: 1
      name: "Skeleton"
      level: 1
      max_health: 50
      experience: 50
      fight {
        damage: 5
        swing_ms: 900
        hit_ms: 650
        reach: 1.2
        sight: 3
        leash: 4
        wander: 3
      }
    }
    spawns { kind: 1 land: "Crypt" x: 6 z: 0 }
  )pb");
  return content;
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

TEST(Copies, KeepAnEmptyDungeonForItsPlayerForAWhile) {
  nanommo::Copies copies(1, {});
  const nanommo::Place gate = GateTo(kCrypt);
  const nanommo::Player alice{
      .name = "alice", .x = gate.x, .z = gate.z, .land = kMeadows};
  copies.Join(7, alice);
  copies.Step(kTick);
  const uint32_t crypt = CopyOf(copies, 7);
  copies.Leave(7);

  // Back in before it goes, she finds the copy she left.
  copies.Step(nanommo::Copies::kEmptyLife - std::chrono::seconds(1));
  copies.Join(7, alice);
  copies.Step(kTick);
  EXPECT_EQ(CopyOf(copies, 7), crypt);

  copies.Leave(7);
  copies.Step(nanommo::Copies::kEmptyLife);
  EXPECT_FALSE(copies.all().contains(crypt));
  EXPECT_EQ(copies.all().size(), 2);
}

TEST(Copies, StopADungeonWithNobodyInIt) {
  nanommo::Copies copies(1, CryptWithASkeleton());
  const nanommo::Place gate = GateTo(kCrypt);
  copies.Join(7, {.name = "alice", .x = gate.x, .z = gate.z, .land = kMeadows});
  copies.Step(kTick);
  const nanommo::Copy& crypt = *copies.Of(7);
  ASSERT_EQ(crypt.world.monsters().size(), 1);
  const nanommo::Monster& skeleton = crypt.world.monsters().begin()->second;

  // With alice in it, the skeleton wanders off.
  const float start_x = skeleton.x;
  const float start_z = skeleton.z;
  RunFor(copies, std::chrono::seconds(10));
  EXPECT_TRUE(skeleton.x != start_x || skeleton.z != start_z);

  // With nobody in it, it stands where it is.
  copies.Leave(7);
  const float left_x = skeleton.x;
  const float left_z = skeleton.z;
  RunFor(copies, std::chrono::seconds(10));
  EXPECT_EQ(skeleton.x, left_x);
  EXPECT_EQ(skeleton.z, left_z);
}
