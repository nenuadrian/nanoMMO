#include "client/nameplates.h"

#include "client/animator.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

TEST(Nameplates, LeaveTheDeadMonstersBare) {
  nanommo::Figure skeleton{.name = "Skeleton", .monster = true};
  EXPECT_TRUE(nanommo::HasNameplate(skeleton));
  skeleton.to = nanommo::protocol::MOTION_DEAD;
  EXPECT_FALSE(nanommo::HasNameplate(skeleton));

  // A player who died keeps theirs, so that the others can tell who it was.
  const nanommo::Figure alice{.name = "alice",
                              .to = nanommo::protocol::MOTION_DEAD};
  EXPECT_TRUE(nanommo::HasNameplate(alice));

  // And one whose name hasn't come yet has nothing to show.
  EXPECT_FALSE(nanommo::HasNameplate(nanommo::Figure{.monster = true}));
}

TEST(Nameplates, MarkTheSwornAndTheHeraldsInTheirFactionsColours) {
  constexpr Color kGold{232, 197, 71, 255};
  // alice has sworn to a faction, and the herald swears players to it.
  const nanommo::Figure alice{
      .name = "alice", .faction = 2, .faction_color = kGold};
  const nanommo::Figure herald{.name = "Herald of Fortuna",
                               .faction = 2,
                               .faction_color = kGold,
                               .npc = true};
  ASSERT_TRUE(nanommo::FactionMark(alice).has_value());
  EXPECT_EQ(nanommo::FactionMark(alice)->r, kGold.r);
  EXPECT_TRUE(nanommo::FactionMark(herald).has_value());

  // bob is in none, and carol in one this client's content hasn't got.
  EXPECT_FALSE(nanommo::FactionMark(nanommo::Figure{.name = "bob"}));
  EXPECT_FALSE(
      nanommo::FactionMark(nanommo::Figure{.name = "carol", .faction = 9}));
}
