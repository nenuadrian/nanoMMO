#include "client/jukebox.h"

#include "gtest/gtest.h"

namespace {

using Cue = nanommo::Jukebox::Cue;

}  // namespace

TEST(Jukebox, FadesInRestsBetweenPlaysAndFadesOut) {
  nanommo::Jukebox jukebox;
  EXPECT_EQ(jukebox.Update(0.0f, true, false), Cue::kStart);
  jukebox.Update(1.5f, true, true);
  EXPECT_FLOAT_EQ(jukebox.volume(), 0.5f);
  jukebox.Update(10.0f, true, true);
  EXPECT_FLOAT_EQ(jukebox.volume(), 1.0f);

  // The track ends, and the music rests before it plays again.
  EXPECT_EQ(jukebox.Update(1.0f, true, false), Cue::kNothing);
  EXPECT_EQ(jukebox.Update(nanommo::Jukebox::kRest - 2.0f, true, false),
            Cue::kNothing);
  EXPECT_EQ(jukebox.Update(1.0f, true, false), Cue::kStart);

  // Leaving the world fades it out, and then stops it.
  jukebox.Update(10.0f, true, true);
  EXPECT_EQ(jukebox.Update(0.75f, false, true), Cue::kNothing);
  EXPECT_FLOAT_EQ(jukebox.volume(), 0.5f);
  EXPECT_EQ(jukebox.Update(0.75f, false, true), Cue::kStop);
}
