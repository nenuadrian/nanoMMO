#include "server/copies.h"

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

TEST(Copies, StartWithACopyOfEachLand) {
  const nanommo::Copies copies(1, {});

  EXPECT_EQ(copies.all().size(), nanommo::kLands.size());
}

TEST(Copies, ShareALand) {
  nanommo::Copies copies(1, {});
  copies.Join(7, {.name = "alice", .land = kMeadows});
  copies.Join(8, {.name = "bob", .land = kMeadows});

  EXPECT_EQ(CopyOf(copies, 7), CopyOf(copies, 8));
  EXPECT_EQ(copies.Of(7)->land, kMeadows);
}

TEST(Copies, TakeAPlayerThroughTheGate) {
  nanommo::Copies copies(1, {});
  const nanommo::Place gate = nanommo::kLands[kMeadows].gate;
  copies.Join(7, {.name = "alice", .x = gate.x, .z = gate.z, .land = kMeadows});
  copies.Step(kTick);

  ASSERT_NE(copies.Of(7), nullptr);
  EXPECT_EQ(copies.Of(7)->land, kAshlands);
  EXPECT_EQ(copies.Of(7)->world.players().at(7).land, kAshlands);
  EXPECT_EQ(copies.players(), 1);
}
