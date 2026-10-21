#include "client/settings.h"

#include <raylib.h>

#include <fstream>
#include <string>

#include "gtest/gtest.h"

namespace {

std::string TestPath(const std::string& name) {
  return testing::TempDir() + "settings/" + name;
}

}  // namespace

TEST(Settings, KeepsTheSoundBetweenRuns) {
  const std::string path = TestPath("kept.txtpb");
  ASSERT_TRUE(nanommo::SaveSettings(
                  path, {.master = 0.5f, .music = 0.25f, .muted = true})
                  .ok());

  const absl::StatusOr<nanommo::SoundSettings> sound =
      nanommo::LoadSettings(path);
  ASSERT_TRUE(sound.ok()) << sound.status();
  EXPECT_EQ(*sound, (nanommo::SoundSettings{
                        .master = 0.5f, .music = 0.25f, .muted = true}));
}

TEST(Settings, TakesTheDefaultsForWhatTheFileLeavesOut) {
  EXPECT_EQ(*nanommo::LoadSettings(TestPath("missing.txtpb")),
            nanommo::SoundSettings());

  const std::string path = TestPath("music.txtpb");
  MakeDirectory(GetDirectoryPath(path.c_str()));
  std::ofstream(path) << "sound { music: 0.25 }";
  const absl::StatusOr<nanommo::SoundSettings> sound =
      nanommo::LoadSettings(path);
  ASSERT_TRUE(sound.ok()) << sound.status();
  EXPECT_EQ(sound->music, 0.25f);
  EXPECT_EQ(sound->master, nanommo::SoundSettings().master);

  std::ofstream(path) << "sound { musik: 0.25 }";
  EXPECT_FALSE(nanommo::LoadSettings(path).ok());
}

TEST(Settings, KeepsTheVolumesBetweenNothingAndFull) {
  const std::string path = TestPath("loud.txtpb");
  MakeDirectory(GetDirectoryPath(path.c_str()));
  std::ofstream(path) << "sound { master: 5 music: -1 }";
  const absl::StatusOr<nanommo::SoundSettings> sound =
      nanommo::LoadSettings(path);
  ASSERT_TRUE(sound.ok()) << sound.status();
  EXPECT_EQ(sound->master, 1.0f);
  EXPECT_EQ(sound->music, 0.0f);

  std::ofstream(path) << "sound { music: nan }";
  const absl::StatusOr<nanommo::SoundSettings> odd =
      nanommo::LoadSettings(path);
  ASSERT_TRUE(odd.ok()) << odd.status();
  EXPECT_EQ(odd->music, nanommo::SoundSettings().music);
}
