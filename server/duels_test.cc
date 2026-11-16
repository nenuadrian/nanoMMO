#include "server/duels.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#include "gtest/gtest.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

// An island, as a duel reaches further than the tests' board. Its players
// start in a clearing, sixteen metres around the middle, where no tree grows.
nanommo::World Island() {
  return nanommo::World(1, nanommo::Terrain(1),
                        nanommo::testing::With(nanommo::testing::Sword()));
}

void Wait(nanommo::World& world, std::chrono::milliseconds time) {
  for (std::chrono::milliseconds run{0}; run < time; run += kTick) {
    world.Step(kTick);
  }
}

}  // namespace

TEST(Duels, AChallengeWaitsForAYes) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice", .x = -2.0f});
  world.Join(2, {.name = "bob", .x = 3.0f});

  // By their name, whatever its case.
  ASSERT_TRUE(world.AskToDuel(1, "BOB").ok());
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text,
            "alice asks you to duel. Type /duel yes to say yes.");
  EXPECT_EQ(notices[1].text, "You asked bob to duel.");
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_ASKING);
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  EXPECT_EQ(world.DuelOf(2).opponent(), "alice");
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));

  // Nothing starts until bob says yes, and alice can't say it for him.
  Wait(world, std::chrono::seconds(10));
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  EXPECT_EQ(world.AnswerDuel(1, true).message(), "Nobody's asked you to duel.");
  ASSERT_TRUE(world.AnswerDuel(2, true).ok());

  // The flag goes down where alice stands, and the countdown starts.
  nanommo::protocol::Duel duel = world.DuelOf(2);
  EXPECT_EQ(duel.state(), nanommo::protocol::DUEL_COUNTDOWN);
  EXPECT_EQ(duel.countdown(), 3000u);
  EXPECT_FLOAT_EQ(duel.flag().x(), -2.0f);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));

  // Both hear as each second goes, and it starts three seconds on.
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.DuelOf(1).countdown(), 2000u);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));
  Wait(world, std::chrono::milliseconds(1950));
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_COUNTDOWN);
  world.Step(kTick);
  duel = world.DuelOf(1);
  EXPECT_EQ(duel.state(), nanommo::protocol::DUEL_FIGHTING);
  EXPECT_EQ(duel.opponent_id(), 2u);
}

TEST(Duels, TooFarToChallenge) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 10.5f});
  EXPECT_EQ(world.AskToDuel(1, "bob").message(),
            "You're too far away to duel bob.");
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_TRUE(world.TakeNotices().empty());
}

TEST(Duels, NearEnoughToChallenge) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 10.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
}

TEST(Duels, AnAskRunsOutAfterThirtySeconds) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 3.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  world.TakeNotices();
  world.TakeDuelNews();

  Wait(world, std::chrono::milliseconds(29950));
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  world.Step(kTick);
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 1u);
  EXPECT_EQ(notices[0].player, 1u);
  EXPECT_EQ(notices[0].text, "bob didn't answer, so there's no duel.");
  EXPECT_EQ(world.AnswerDuel(2, true).message(), "Nobody's asked you to duel.");
}

TEST(Duels, LeavingTheFlagLosesTheDuel) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 3.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  ASSERT_TRUE(world.AnswerDuel(2, true).ok());
  Wait(world, nanommo::Duels::kCountdown);
  ASSERT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_FIGHTING);
  world.TakeNotices();

  // Bob walks off, round any trees, and the duel's on until he's more than
  // 25 metres from the flag, where alice stood.
  const nanommo::Place away{.x = 40.0f, .z = 0.0f};
  ASSERT_TRUE(world.terrain().Walkable(away.x, away.z));
  world.WalkTo(2, away);
  const auto from_flag = [&world] {
    const nanommo::Player& bob = world.players().at(2);
    return std::hypot(bob.x, bob.z);
  };
  for (int tick = 0; from_flag() <= nanommo::Duels::kFlagReach; ++tick) {
    ASSERT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_FIGHTING);
    ASSERT_LT(tick, 400) << "bob didn't get far enough";
    world.Step(kTick);
  }
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_NONE);
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 3u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text, "You went too far from the flag.");
  EXPECT_EQ(notices[1].text, "alice won the duel.");
  EXPECT_EQ(notices[2].player, 1u);
  EXPECT_EQ(notices[2].text, "You won the duel against bob.");
}
