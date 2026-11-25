#include "tools/cheat/measure.h"

#include "gtest/gtest.h"

TEST(Speedometer, MeasuresTheLastSecondOfWorlds) {
  nanommo::Speedometer speed;
  EXPECT_EQ(speed.MetresASecond(), 0.0f);
  // Twenty worlds a second, for two seconds, running along x.
  for (int tick = 0; tick <= 40; ++tick) {
    speed.Add(tick * 0.05, static_cast<float>(tick) * 0.05f * 3.1f, 0.0f);
  }
  EXPECT_NEAR(speed.MetresASecond(), 3.1f, 1e-3f);
}

TEST(Speedometer, CountsTheWayThereAndBack) {
  nanommo::Speedometer speed;
  // Three metres out and three back, in two seconds.
  speed.Add(0.0, 0.0f, 0.0f);
  speed.Add(1.0, 3.0f, 0.0f);
  speed.Add(2.0, 0.0f, 0.0f);
  EXPECT_NEAR(speed.MetresASecond(), 3.0f, 1e-4f);
}

TEST(Speedometer, LeavesOutAWorldThatCameLate) {
  nanommo::Speedometer speed;
  speed.Add(1.0, 0.0f, 0.0f);
  speed.Add(2.0, 3.0f, 0.0f);
  // Sent before the newest, so it's old news.
  speed.Add(1.5, 100.0f, 0.0f);
  EXPECT_NEAR(speed.MetresASecond(), 3.0f, 1e-4f);
}

TEST(Hits, CountsTheLastFiveSeconds) {
  nanommo::Hits hits;
  // A hit of 10 each second, for eight seconds.
  for (int second = 1; second <= 8; ++second) {
    hits.Add(second, 10);
  }
  EXPECT_FLOAT_EQ(hits.PerSecond(8.0), 1.0f);
  EXPECT_FLOAT_EQ(hits.DamagePerSecond(8.0), 10.0f);
  // Three seconds on, two of the five seconds had hits.
  EXPECT_FLOAT_EQ(hits.PerSecond(11.0), 0.4f);
}
