#include "bot/wander.h"

#include <chrono>
#include <optional>
#include <random>
#include <set>
#include <utility>

#include "gtest/gtest.h"
#include "terrain/lands.h"

TEST(Wander, HoldsDirectionsLikeTheKeysWould) {
  std::mt19937 random(1);
  std::set<std::pair<float, float>> seen;
  for (int i = 0; i < 1000; ++i) {
    const nanommo::protocol::Input input = nanommo::RandomDirection(random);
    seen.insert({input.x(), input.z()});
  }

  EXPECT_EQ(seen.size(), 9u);
  for (const auto& [x, z] : seen) {
    EXPECT_TRUE(x == -1.0f || x == 0.0f || x == 1.0f);
    EXPECT_TRUE(z == -1.0f || z == 0.0f || z == 1.0f);
  }
}

TEST(Wander, RunsAboutAThirdOfTheTime) {
  std::mt19937 random(1);
  int runs = 0;
  for (int i = 0; i < 1000; ++i) {
    runs += nanommo::RandomDirection(random).run() ? 1 : 0;
  }

  EXPECT_GT(runs, 280);
  EXPECT_LT(runs, 390);
}

TEST(Wander, HoldsForHalfASecondToThreeSeconds) {
  std::mt19937 random(1);
  for (int i = 0; i < 1000; ++i) {
    const auto hold = nanommo::RandomHold(random);
    EXPECT_GE(hold, std::chrono::milliseconds(500));
    EXPECT_LE(hold, std::chrono::milliseconds(3000));
  }
}

TEST(Wander, TravelsUntilItArrivesOrStopsGettingCloser) {
  nanommo::Trip trip{.to_x = 10.0f};
  const std::optional<nanommo::protocol::Input> input =
      nanommo::Travel(trip, 0.0f, 0.0f);
  ASSERT_TRUE(input.has_value());
  EXPECT_FLOAT_EQ(input->x(), 1.0f);
  EXPECT_FLOAT_EQ(input->z(), 0.0f);
  EXPECT_TRUE(input->run());
  EXPECT_FALSE(nanommo::Travel(trip, 8.0f, 0.0f).has_value());

  nanommo::Trip blocked{.to_x = 10.0f};
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(nanommo::Travel(blocked, 4.0f, 0.0f).has_value());
  }
  EXPECT_FALSE(nanommo::Travel(blocked, 4.0f, 0.0f).has_value());
}

TEST(Wander, GoesIntoTheGateOnATripInFive) {
  std::mt19937 random(1);
  const nanommo::Place gate{.x = 0.0f, .z = -10.0f};
  int through = 0;
  for (int i = 0; i < 1000; ++i) {
    const nanommo::Trip trip = nanommo::RandomTrip(random, gate);
    if (trip.to_x == gate.x && trip.to_z == gate.z) {
      ++through;
      EXPECT_LT(trip.there, nanommo::kGateRadius);
    }
  }

  EXPECT_GT(through, 150);
  EXPECT_LT(through, 250);
}

TEST(Wander, StepsOutOfAMonstersSkillOnItsWay) {
  const nanommo::MonsterTable table = *nanommo::ParseMonsters(R"pb(
    kinds {
      id: 2
      skills {
        id: 1
        name: "Ground Slam"
        size: 3
        range: 1.5
        telegraph_ms: 2000
        cooldown_ms: 10000
      }
    }
  )pb");
  nanommo::protocol::World world;
  nanommo::protocol::Monster* skeleton = world.add_monsters();
  skeleton->set_kind(2);
  skeleton->set_x(10.0f);
  skeleton->set_motion(nanommo::protocol::MOTION_SKILL);
  skeleton->set_skill(1);

  // A metre to its left, the bot runs left, out of the three metres round it.
  const std::optional<nanommo::protocol::Input> out =
      nanommo::StepOut(table, world, 9.0f, 0.0f);
  ASSERT_TRUE(out.has_value());
  EXPECT_FLOAT_EQ(out->x(), -1.0f);
  EXPECT_FLOAT_EQ(out->z(), 0.0f);
  EXPECT_TRUE(out->run());
  EXPECT_FALSE(out->attack());
  // Just out, it goes on for a metre more, and then it's clear.
  EXPECT_TRUE(nanommo::StepOut(table, world, 6.5f, 0.0f).has_value());
  EXPECT_FALSE(nanommo::StepOut(table, world, 5.5f, 0.0f).has_value());

  // Once it's landed, there's nothing to step out of.
  skeleton->set_motion(nanommo::protocol::MOTION_ATTACK);
  EXPECT_FALSE(nanommo::StepOut(table, world, 9.0f, 0.0f).has_value());
}
