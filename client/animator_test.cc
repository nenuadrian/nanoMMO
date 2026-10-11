#include "client/animator.h"

#include <numbers>

#include "gtest/gtest.h"

namespace {

nanommo::protocol::World WorldWith(float x, float z, float yaw,
                                   nanommo::protocol::Motion motion) {
  nanommo::protocol::World world;
  world.set_you(1);
  nanommo::protocol::Player* player = world.add_players();
  player->set_id(1);
  player->set_name("alice");
  player->set_x(x);
  player->set_z(z);
  player->set_yaw(yaw);
  player->set_motion(motion);
  return world;
}

}  // namespace

TEST(Animator, BlendsIntoTheNextClip) {
  nanommo::Animator animator;
  animator.Update(WorldWith(0, 0, 0, nanommo::protocol::MOTION_WALK), 0.5f);

  auto figures =
      animator.Update(WorldWith(0, 0, 0, nanommo::protocol::MOTION_RUN), 0.1f);
  EXPECT_EQ(figures[0].from, nanommo::protocol::MOTION_WALK);
  EXPECT_EQ(figures[0].to, nanommo::protocol::MOTION_RUN);
  EXPECT_FLOAT_EQ(figures[0].from_time, 0.6f);
  EXPECT_FLOAT_EQ(figures[0].blend, 0.4f);

  figures =
      animator.Update(WorldWith(0, 0, 0, nanommo::protocol::MOTION_RUN), 0.2f);
  EXPECT_FLOAT_EQ(figures[0].to_time, 0.2f);
  EXPECT_EQ(figures[0].blend, 1.0f);
}

TEST(Animator, CatchesUpWithTheServerOverAFewFrames) {
  nanommo::Animator animator;
  animator.Update(WorldWith(0, 0, 0, nanommo::protocol::MOTION_WALK), 0.0f);

  const auto world = WorldWith(1.0f, 0, 0, nanommo::protocol::MOTION_WALK);
  float x = animator.Update(world, 1.0f / 60).at(0).position.x;
  EXPECT_GT(x, 0.1f);
  EXPECT_LT(x, 0.5f);
  for (int frame = 1; frame < 15; ++frame) {
    x = animator.Update(world, 1.0f / 60).at(0).position.x;
  }
  EXPECT_NEAR(x, 1.0f, 0.03f);
}

TEST(Animator, TurnsTheShortWayRound) {
  const float pi = std::numbers::pi_v<float>;
  nanommo::Animator animator;
  animator.Update(WorldWith(0, 0, pi - 0.1f, nanommo::protocol::MOTION_WALK),
                  0);

  const auto figures = animator.Update(
      WorldWith(0, 0, -pi + 0.1f, nanommo::protocol::MOTION_WALK), 0.1f);
  EXPECT_NEAR(figures[0].yaw, pi + 0.1f, 0.001f);
}
