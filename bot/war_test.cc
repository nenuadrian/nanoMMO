#include "bot/war.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

// bot1 leads Red, against Blue, and asks in bot3 and bot5.
const nanommo::Side kLeader{.guild = "Red",
                            .enemy = "Blue",
                            .recruits = {"bot3", "bot5"},
                            .leads = true};

nanommo::protocol::Guild Red(bool at_war) {
  nanommo::protocol::Guild guild;
  guild.set_name("Red");
  guild.add_members()->set_name("bot1");
  if (at_war) {
    guild.mutable_war()->set_enemy("Blue");
  }
  return guild;
}

}  // namespace

TEST(War, TheLeaderFoundsTheGuildGoesToWarAndAsksItsSideIn) {
  std::size_t next = 0;
  const std::optional<nanommo::protocol::GuildAction> founds =
      nanommo::Enlist(kLeader, {}, next);
  ASSERT_TRUE(founds);
  EXPECT_EQ(founds->create(), "Red");
  const std::optional<nanommo::protocol::GuildAction> asks =
      nanommo::Enlist(kLeader, Red(false), next);
  ASSERT_TRUE(asks);
  EXPECT_EQ(asks->war(), "Blue");

  // At war, each of its side, one at a time, round the list, past those who
  // are in.
  nanommo::protocol::Guild red = Red(true);
  EXPECT_EQ(nanommo::Enlist(kLeader, red, next)->invite(), "bot3");
  EXPECT_EQ(nanommo::Enlist(kLeader, red, next)->invite(), "bot5");
  red.add_members()->set_name("bot5");
  EXPECT_EQ(nanommo::Enlist(kLeader, red, next)->invite(), "bot3");
  EXPECT_EQ(nanommo::Enlist(kLeader, red, next)->invite(), "bot3");
  red.add_members()->set_name("bot3");
  EXPECT_FALSE(nanommo::Enlist(kLeader, red, next));
}

TEST(War, ALeaderWithNobodyElseOnItsSideStillGoesToWar) {
  // With two bots, bot1 leads Red and bot2 Blue, each on its own.
  const nanommo::Side alone{.guild = "Red", .enemy = "Blue", .leads = true};
  std::size_t next = 0;
  EXPECT_EQ(nanommo::Enlist(alone, {}, next)->create(), "Red");
  EXPECT_EQ(nanommo::Enlist(alone, Red(false), next)->war(), "Blue");
  EXPECT_FALSE(nanommo::Enlist(alone, Red(true), next));
}

TEST(War, TheRestWaitToBeAskedIn) {
  std::size_t next = 0;
  EXPECT_FALSE(nanommo::Enlist({.guild = "Red", .enemy = "Blue"}, {}, next));
}

TEST(War, GoesForTheNearestEnemyAlive) {
  nanommo::protocol::World seen;
  const auto player = [&](uint32_t id, const char* guild, float x,
                          bool dead = false) {
    nanommo::protocol::Player* added = seen.add_players();
    added->set_id(id);
    added->set_guild(guild);
    added->set_x(x);
    added->set_motion(dead ? nanommo::protocol::MOTION_DEAD
                           : nanommo::protocol::MOTION_IDLE);
  };
  player(1, "Red", 0.0f);
  player(2, "Red", 1.0f);
  player(3, "", 2.0f);
  player(4, "Ash Hounds", 3.0f);
  player(5, "Blue", 4.0f, true);
  player(6, "Blue", -9.0f);
  player(7, "Blue", 8.0f);

  EXPECT_EQ(nanommo::NearestEnemy(kLeader, seen, 0.0f, 0.0f), 7u);
  EXPECT_EQ(nanommo::NearestEnemy(kLeader, seen, -20.0f, 0.0f), 6u);
  EXPECT_EQ(nanommo::NearestEnemy({.guild = "Blue", .enemy = "Red"}, seen, 0.0f,
                                  0.0f),
            1u);
}
