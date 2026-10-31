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
