#include "server/shown.h"

#include <chrono>
#include <optional>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"
#include "server/world.h"

TEST(Shown, TellsAPlayerTheirOwnPartWithTheLastInputApplied) {
  nanommo::Player player{.name = "alice", .x = 2.0f, .z = -1.0f};
  player.skills = {3};

  const nanommo::protocol::World you =
      nanommo::YouIn(7, player, /*seed=*/99, /*land=*/1, /*acked=*/12);
  EXPECT_EQ(you.you(), 7u);
  EXPECT_EQ(you.seed(), 99u);
  EXPECT_EQ(you.land(), 1u);
  EXPECT_EQ(you.acked(), 12u);
  ASSERT_EQ(you.skills_size(), 1);
  EXPECT_EQ(you.skills(0), 3u);
  // Where they are goes with it, without what the world sends now and then.
  ASSERT_EQ(you.players_size(), 1);
  EXPECT_FLOAT_EQ(you.players(0).x(), 2.0f);
  EXPECT_TRUE(you.players(0).name().empty());

  // Before any numbered input's applied, there's none to say.
  EXPECT_EQ(nanommo::YouIn(7, player, 99, 1, std::nullopt).acked(), 0u);

  // What they're making, and how far it's got, while they're making it.
  EXPECT_FALSE(you.has_crafting());
  player.crafting = 2;
  player.crafted = std::chrono::milliseconds(4000);
  const nanommo::protocol::World making =
      nanommo::YouIn(7, player, 99, 1, std::nullopt);
  EXPECT_EQ(making.crafting().recipe(), 2u);
  EXPECT_EQ(making.crafting().done(), 4000u);
}

TEST(Shown, ShowsThePlayersTheBlowsAndNotTheirQuests) {
  const std::optional<nanommo::protocol::Event> hit =
      nanommo::ForPlayers({.kind = nanommo::Event::Kind::kHit,
                           .player_id = 3,
                           .monster_id = 9,
                           .amount = 15});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->kind(), nanommo::protocol::EVENT_HIT);
  EXPECT_EQ(hit->player(), 3u);
  EXPECT_EQ(hit->monster(), 9u);
  EXPECT_EQ(hit->amount(), 15u);

  // A level's amount is the level reached.
  const std::optional<nanommo::protocol::Event> level = nanommo::ForPlayers(
      {.kind = nanommo::Event::Kind::kLevel, .player_id = 3, .level = 7});
  ASSERT_TRUE(level.has_value());
  EXPECT_EQ(level->kind(), nanommo::protocol::EVENT_LEVEL);
  EXPECT_EQ(level->amount(), 7u);

  EXPECT_FALSE(
      nanommo::ForPlayers({.kind = nanommo::Event::Kind::kTake, .player_id = 3})
          .has_value());
}
