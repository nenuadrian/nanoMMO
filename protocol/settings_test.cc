#include "protocol/settings.h"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <string>

#include "gtest/gtest.h"

namespace {

bool Exists(const std::string& path) { return access(path.c_str(), F_OK) == 0; }

// Runs each test from a temporary folder, outside the repository, and goes
// back after.
class FileSettingTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NE(getcwd(here_.data(), here_.size()), nullptr);
    ASSERT_EQ(chdir(testing::TempDir().c_str()), 0);
  }
  void TearDown() override {
    ASSERT_EQ(chdir(here_.data()), 0);
    unsetenv("NANOMMO_TEST_FOLDER");
  }

 private:
  std::array<char, 4096> here_{};
};

TEST_F(FileSettingTest, FindsTheRepositorysFilesFromAnotherFolder) {
  const std::string content =
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "content");
  EXPECT_TRUE(Exists(content + "/items.txtpb")) << content;
}

TEST_F(FileSettingTest, TakesTheFolderItRunsInForWhatTheRepositoryHasnt) {
  mkdir("nanommo-settings-test", 0755);
  EXPECT_EQ(
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "nanommo-settings-test"),
      "nanommo-settings-test");
  rmdir("nanommo-settings-test");
}

TEST_F(FileSettingTest, TakesTheSettingOverEither) {
  setenv("NANOMMO_TEST_FOLDER", "/elsewhere/content", 1);
  EXPECT_EQ(nanommo::FileSetting("NANOMMO_TEST_FOLDER", "content"),
            "/elsewhere/content");
}

TEST_F(FileSettingTest, PointsAtTheRepositoryForWhatsMissing) {
  const std::string key =
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "tls/not-there.key");
  EXPECT_TRUE(key.starts_with("/")) << key;
  EXPECT_TRUE(key.ends_with("/tls/not-there.key")) << key;
}

}  // namespace
