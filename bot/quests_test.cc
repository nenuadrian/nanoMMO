#include "bot/quests.h"

#include <cstdint>
#include <optional>

#include "content/content.h"
#include "gtest/gtest.h"

namespace {

using nanommo::Errand;

// Bones by the Gate: the Guard's, for three Skeletons.
constexpr uint32_t kBones = 1;
constexpr uint32_t kGuard = 1;
constexpr uint32_t kSkeleton = 1;
constexpr uint32_t kAshSkeleton = 2;

nanommo::Content Content() { return *nanommo::ReadContent(NANOMMO_CONTENT); }

// The guard, with an id of its own, at (10, 0), and two Skeletons and an Ash
// Skeleton around the bot at the middle.
nanommo::protocol::World Seen() {
  nanommo::protocol::World seen;
  nanommo::protocol::Npc* guard = seen.add_npcs();
  guard->set_id(7);
  guard->set_kind(kGuard);
  guard->set_x(10.0f);
  const auto monster = [&](uint32_t kind, float x) {
    nanommo::protocol::Monster* added = seen.add_monsters();
    added->set_kind(kind);
    added->set_x(x);
  };
  monster(kSkeleton, 20.0f);
  monster(kAshSkeleton, 2.0f);
  monster(kSkeleton, -8.0f);
  return seen;
}

}  // namespace

TEST(Quests, TakesAQuestFromItsGiver) {
  const std::optional<Errand> errand =
      nanommo::NextErrand(Content(), {}, Seen(), 0.0f, 0.0f);
  ASSERT_TRUE(errand);
  EXPECT_EQ(errand->kind, Errand::Kind::kTake);
  EXPECT_EQ(errand->quest.npc(), 7u);
  EXPECT_EQ(errand->quest.quest(), kBones);
  EXPECT_FLOAT_EQ(errand->x, 10.0f);
}

TEST(Quests, HuntsTheNearestMonsterItsQuestAsksFor) {
  nanommo::protocol::Quests quests;
  (*quests.mutable_on())[kBones] = 1;

  const std::optional<Errand> errand =
      nanommo::NextErrand(Content(), quests, Seen(), 0.0f, 0.0f);
  ASSERT_TRUE(errand);
  EXPECT_EQ(errand->kind, Errand::Kind::kHunt);
  // Not the Ash Skeleton, nearer as it is.
  EXPECT_FLOAT_EQ(errand->x, -8.0f);

  nanommo::protocol::World dead = Seen();
  for (nanommo::protocol::Monster& monster : *dead.mutable_monsters()) {
    monster.set_motion(nanommo::protocol::MOTION_DEAD);
  }
  EXPECT_FALSE(nanommo::NextErrand(Content(), quests, dead, 0.0f, 0.0f));
}

TEST(Quests, TurnsInAQuestItsDone) {
  nanommo::protocol::Quests quests;
  (*quests.mutable_on())[kBones] = 3;

  const std::optional<Errand> errand =
      nanommo::NextErrand(Content(), quests, Seen(), 0.0f, 0.0f);
  ASSERT_TRUE(errand);
  EXPECT_EQ(errand->kind, Errand::Kind::kTurnIn);
  EXPECT_EQ(errand->quest.npc(), 7u);
  EXPECT_EQ(errand->quest.quest(), kBones);
}

TEST(Quests, LeavesAQuestItsDone) {
  nanommo::protocol::Quests quests;
  quests.add_done(kBones);

  EXPECT_FALSE(nanommo::NextErrand(Content(), quests, Seen(), 0.0f, 0.0f));
}
