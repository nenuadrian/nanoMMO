#include "client/animator.h"

#include <cstdint>
#include <numbers>
#include <vector>

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

TEST(Animator, KeepsPlayersAndMonstersApart) {
  nanommo::protocol::World world =
      WorldWith(0, 0, 0, nanommo::protocol::MOTION_IDLE);
  nanommo::protocol::Monster* monster = world.add_monsters();
  monster->set_id(1);
  monster->set_kind(1);
  nanommo::Content content;
  content.monsters =
      *nanommo::ParseMonsters(R"pb(kinds { id: 1 name: "Skeleton" })pb");

  const auto figures = nanommo::Animator(content).Update(world, 0.1f);
  ASSERT_EQ(figures.size(), 2u);
  EXPECT_FALSE(figures[0].monster);
  EXPECT_TRUE(figures[1].monster);
  EXPECT_EQ(figures[1].name, "Skeleton");
}

TEST(Animator, DressesAnNpcAsItsKind) {
  nanommo::protocol::World world;
  nanommo::protocol::Npc* npc = world.add_npcs();
  npc->set_id(1);
  npc->set_kind(1);
  nanommo::Content content;
  content.npcs = *nanommo::ParseNpcs(R"pb(
    kinds {
      id: 1
      name: "Guard"
      look {
        color: 0x3E6FD8FF
        skin: 0xF1C49CFF
        hair_color: 0x1C181AFF
        hair: 3
        weapon: 1
        wearing: [ 4, 5 ]
      }
    }
  )pb");

  const auto figures = nanommo::Animator(content).Update(world, 0.1f);
  ASSERT_EQ(figures.size(), 1u);
  EXPECT_TRUE(figures[0].npc);
  EXPECT_EQ(figures[0].name, "Guard");
  ASSERT_TRUE(figures[0].features.has_value());
  EXPECT_EQ(figures[0].features->skin, 0xF1C49CFFu);
  EXPECT_EQ(figures[0].features->hair_color, 0x1C181AFFu);
  EXPECT_EQ(figures[0].features->hair, 3);
  EXPECT_EQ(figures[0].weapon, 1u);
  EXPECT_EQ(figures[0].wearing, (std::vector<uint32_t>{4, 5}));
}
