#include "client/interpolation.h"

#include "gtest/gtest.h"

namespace {

// A world sent at the time, in seconds on the server's clock, with player 1
// and player 2, who's you, at x.
nanommo::protocol::World At(double sent, float x) {
  nanommo::protocol::World world;
  world.set_sent(static_cast<int64_t>(sent * 1e6));
  world.set_you(2);
  for (const uint32_t id : {1u, 2u}) {
    nanommo::protocol::Player* player = world.add_players();
    player->set_id(id);
    player->set_x(x);
  }
  return world;
}

}  // namespace

TEST(Interpolation, DrawsOthersBetweenTheWorldsAMomentAgo) {
  nanommo::Interpolation interpolation;
  // The server's clock is 1000 s behind ours, and each world takes 30 ms.
  interpolation.Add(At(5.00, 0.0f), 1005.03);
  interpolation.Add(At(5.05, 1.0f), 1005.08);
  const nanommo::protocol::World newest = At(5.10, 2.0f);
  interpolation.Add(newest, 1005.13);

  // At 1005.155 on our clock, kDelay ago on the server's is 5.025: halfway
  // from the first world to the second.
  const nanommo::protocol::World shown = interpolation.Shown(newest, 1005.155);
  EXPECT_NEAR(shown.players(0).x(), 0.5f, 1e-4f);
  // You're the prediction's, so where the newest world has you.
  EXPECT_FLOAT_EQ(shown.players(1).x(), 2.0f);
}

TEST(Interpolation, LearnsTheClocksFromTheQuickestWorld) {
  nanommo::Interpolation interpolation;
  // The second world was held up 40 ms more on the way than the others.
  interpolation.Add(At(5.00, 0.0f), 1005.03);
  interpolation.Add(At(5.05, 1.0f), 1005.12);
  const nanommo::protocol::World newest = At(5.10, 2.0f);
  interpolation.Add(newest, 1005.13);

  EXPECT_NEAR(interpolation.Shown(newest, 1005.155).players(0).x(), 0.5f,
              1e-4f);
}

TEST(Interpolation, FillsInAWorldAsItsPartsCome) {
  nanommo::Interpolation interpolation;
  interpolation.Add(At(5.00, 0.0f), 1005.03);
  // The next world's first part has nobody in it, and its second, a frame
  // later, has them.
  nanommo::protocol::World first_part = At(5.05, 1.0f);
  first_part.clear_players();
  interpolation.Add(first_part, 1005.08);
  const nanommo::protocol::World whole = At(5.05, 1.0f);
  interpolation.Add(whole, 1005.10);

  // Halfway between the two worlds, with the clock from the first part.
  EXPECT_NEAR(interpolation.Shown(whole, 1005.155).players(0).x(), 0.5f, 1e-4f);
}
