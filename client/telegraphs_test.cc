#include "client/telegraphs.h"

#include <cstdint>

#include "content/monsters.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

// An Ash Skeleton, whose slam shows for two seconds.
nanommo::MonsterTable AshSkeleton() {
  return *nanommo::ParseMonsters(R"pb(
    kinds {
      id: 2
      name: "Ash Skeleton"
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
}

// A world with the skeleton in it, where it stands, facing a radian round
// from z, with the slam on its way, or with whatever its motion is.
nanommo::protocol::World Slamming(
    float x, uint32_t lands_in,
    nanommo::protocol::Motion motion = nanommo::protocol::MOTION_SKILL) {
  nanommo::protocol::World world;
  nanommo::protocol::Monster* skeleton = world.add_monsters();
  skeleton->set_id(7);
  skeleton->set_kind(2);
  skeleton->set_x(x);
  skeleton->set_yaw(1.0f);
  skeleton->set_motion(motion);
  if (motion == nanommo::protocol::MOTION_SKILL) {
    skeleton->set_skill(1);
    skeleton->set_lands_in(lands_in);
  }
  return world;
}

}  // namespace

TEST(Telegraphs, FillUntilTheSkillLandsWhereTheMonsterAimedIt) {
  nanommo::Telegraphs telegraphs(AshSkeleton());
  telegraphs.Update(Slamming(2.0f, 2000), 0.0f);
  ASSERT_TRUE(telegraphs.shown().contains(7));
  const nanommo::Telegraph& slam = telegraphs.shown().at(7);
  EXPECT_EQ(slam.skill.name(), "Ground Slam");
  EXPECT_FLOAT_EQ(slam.at.x, 2.0f);
  EXPECT_FLOAT_EQ(slam.yaw, 1.0f);
  EXPECT_FLOAT_EQ(slam.Filled(), 0.0f);

  // Between worlds, it fills by itself, a frame at a time.
  telegraphs.Update(Slamming(2.0f, 2000), 0.5f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).Filled(), 0.25f);
  // A world that comes late doesn't empty it again, and the next on time
  // puts it where the server has it.
  telegraphs.Update(Slamming(2.0f, 1600), 0.0f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).Filled(), 0.25f);
  telegraphs.Update(Slamming(2.0f, 1000), 0.0f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).Filled(), 0.5f);

  // A kick knocks the skeleton three metres back, and the shape stays where
  // it was aimed, as it does on the server.
  telegraphs.Update(Slamming(5.0f, 950), 0.05f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).at.x, 2.0f);
}

TEST(Telegraphs, GoOnceTheSkillLandsOrTheMonsterDies) {
  nanommo::Telegraphs telegraphs(AshSkeleton());
  telegraphs.Update(Slamming(0.0f, 50), 0.0f);
  telegraphs.Update(Slamming(0.0f, 0, nanommo::protocol::MOTION_ATTACK), 0.05f);
  EXPECT_TRUE(telegraphs.shown().empty());

  telegraphs.Update(Slamming(0.0f, 1500), 0.0f);
  telegraphs.Update(Slamming(0.0f, 0, nanommo::protocol::MOTION_DEAD), 0.05f);
  EXPECT_TRUE(telegraphs.shown().empty());

  // And once the skeleton's too far away to be sent.
  telegraphs.Update(Slamming(0.0f, 1500), 0.0f);
  telegraphs.Update(nanommo::protocol::World(), 0.05f);
  EXPECT_TRUE(telegraphs.shown().empty());
}

TEST(Telegraphs, TheNextSkillStartsAfresh) {
  nanommo::Telegraphs telegraphs(AshSkeleton());
  telegraphs.Update(Slamming(0.0f, 100), 0.0f);
  // Straight after the last, from where the skeleton stands now.
  telegraphs.Update(Slamming(1.0f, 2000), 0.05f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).Filled(), 0.0f);
  EXPECT_FLOAT_EQ(telegraphs.shown().at(7).at.x, 1.0f);

  // A skill the content doesn't have, from a newer server, isn't drawn.
  nanommo::protocol::World newer = Slamming(0.0f, 2000);
  newer.mutable_monsters(0)->set_skill(9);
  telegraphs.Update(newer, 0.05f);
  EXPECT_TRUE(telegraphs.shown().empty());
}
