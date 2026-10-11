#include "server/world.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>

#include "gtest/gtest.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

void RunOneSecond(nanommo::World& world) {
  for (int tick = 0; tick < 20; ++tick) {
    world.Step(kTick);
  }
}

}  // namespace

TEST(World, JoinsPlayersWhereTheyAre) {
  nanommo::World world;
  world.Join(7, {.name = "alice", .x = 2.0f, .z = -1.0f});

  const nanommo::Player& player = world.players().at(7);
  EXPECT_EQ(player.name, "alice");
  EXPECT_EQ(player.x, 2.0f);
  EXPECT_EQ(player.z, -1.0f);
}

TEST(World, WalksAtTheWalkSpeed) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const float start = world.players().at(id).x;

  world.SetInput(id, 1.0f, 0.0f, false);
  RunOneSecond(world);

  EXPECT_FLOAT_EQ(world.players().at(id).x, start + nanommo::World::kWalkSpeed);
  EXPECT_EQ(world.players().at(id).motion, nanommo::Motion::kWalk);
}

TEST(World, DiagonalsAreNoFaster) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const nanommo::Player start = world.players().at(id);

  world.SetInput(id, 1.0f, 1.0f, false);
  RunOneSecond(world);

  const nanommo::Player& end = world.players().at(id);
  EXPECT_FLOAT_EQ(std::hypot(end.x - start.x, end.z - start.z),
                  nanommo::World::kWalkSpeed);
}

TEST(World, FacesTheWayItMoves) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});

  world.SetInput(id, 1.0f, 0.0f, false);
  world.Step(kTick);
  EXPECT_FLOAT_EQ(world.players().at(id).yaw, std::numbers::pi_v<float> / 2);

  world.SetInput(id, 0.0f, -1.0f, false);
  world.Step(kTick);
  EXPECT_FLOAT_EQ(world.players().at(id).yaw, std::numbers::pi_v<float>);
}

TEST(World, KeepsPlayersOnTheGround) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});

  world.SetInput(id, 0.0f, -1.0f, true);
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }

  EXPECT_EQ(world.players().at(id).z, -nanommo::World::kEdge);
  EXPECT_EQ(world.players().at(id).motion, nanommo::Motion::kIdle);
}

TEST(World, IgnoresInputThatIsNotANumber) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const float start = world.players().at(id).x;

  world.SetInput(id, std::numeric_limits<float>::quiet_NaN(), 0.0f, false);
  RunOneSecond(world);

  EXPECT_EQ(world.players().at(id).x, start);
}
