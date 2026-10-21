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

TEST(Settings, KeepsThemBetweenRuns) {
  const std::string path = TestPath("kept.txtpb");
  const nanommo::Settings settings = {
      .sound = {.master = 0.5f, .music = 0.25f, .muted = true},
      .graphics = {.bloom = false, .cel_shading = true, .antialias = false}};
  ASSERT_TRUE(nanommo::SaveSettings(path, settings).ok());

  const absl::StatusOr<nanommo::Settings> loaded = nanommo::LoadSettings(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(*loaded, settings);
}

TEST(Settings, TakesTheDefaultsForWhatTheFileLeavesOut) {
  EXPECT_EQ(*nanommo::LoadSettings(TestPath("missing.txtpb")),
            nanommo::Settings());

  const std::string path = TestPath("music.txtpb");
  MakeDirectory(GetDirectoryPath(path.c_str()));
  std::ofstream(path) << "sound { music: 0.25 }";
  const absl::StatusOr<nanommo::Settings> loaded = nanommo::LoadSettings(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->sound.music, 0.25f);
  EXPECT_EQ(loaded->sound.master, nanommo::SoundSettings().master);
  EXPECT_EQ(loaded->graphics, nanommo::GraphicsSettings());

  std::ofstream(path) << "sound { musik: 0.25 }";
  EXPECT_FALSE(nanommo::LoadSettings(path).ok());
}

TEST(Settings, KeepsTheVolumesBetweenNothingAndFull) {
  const std::string path = TestPath("loud.txtpb");
  MakeDirectory(GetDirectoryPath(path.c_str()));
  std::ofstream(path) << "sound { master: 5 music: -1 }";
  const absl::StatusOr<nanommo::Settings> loaded = nanommo::LoadSettings(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->sound.master, 1.0f);
  EXPECT_EQ(loaded->sound.music, 0.0f);

  std::ofstream(path) << "sound { music: nan }";
  const absl::StatusOr<nanommo::Settings> odd = nanommo::LoadSettings(path);
  ASSERT_TRUE(odd.ok()) << odd.status();
  EXPECT_EQ(odd->sound.music, nanommo::SoundSettings().music);
}
