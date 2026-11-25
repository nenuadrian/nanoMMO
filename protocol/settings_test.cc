#include "protocol/settings.h"

#include <cstdlib>
// POSIX's getcwd, chdir and mkdir are different on Windows.
#include <filesystem>  // NOLINT(build/c++17)
#include <string>

#include "gtest/gtest.h"

namespace {

bool Exists(const std::string& path) { return std::filesystem::exists(path); }

void SetEnvironment(const char* name, const char* value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

// On Windows, setting a variable to nothing takes it away.
void UnsetEnvironment(const char* name) {
#if defined(_WIN32)
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

// Runs each test from a temporary folder, outside the repository, and goes
// back after.
class FileSettingTest : public testing::Test {
 protected:
  void SetUp() override {
    here_ = std::filesystem::current_path();
    std::filesystem::current_path(testing::TempDir());
  }
  void TearDown() override {
    std::filesystem::current_path(here_);
    UnsetEnvironment("NANOMMO_TEST_FOLDER");
  }

 private:
  std::filesystem::path here_;
};

TEST_F(FileSettingTest, FindsTheRepositorysFilesFromAnotherFolder) {
  const std::string content =
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "content");
  EXPECT_TRUE(Exists(content + "/items.txtpb")) << content;
}

TEST_F(FileSettingTest, TakesTheFolderItRunsInForWhatTheRepositoryHasnt) {
  std::filesystem::create_directory("nanommo-settings-test");
  EXPECT_EQ(
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "nanommo-settings-test"),
      "nanommo-settings-test");
  std::filesystem::remove("nanommo-settings-test");
}

TEST_F(FileSettingTest, TakesTheSettingOverEither) {
  SetEnvironment("NANOMMO_TEST_FOLDER", "/elsewhere/content");
  EXPECT_EQ(nanommo::FileSetting("NANOMMO_TEST_FOLDER", "content"),
            "/elsewhere/content");
}

TEST_F(FileSettingTest, PointsAtTheRepositoryForWhatsMissing) {
  const std::string key =
      nanommo::FileSetting("NANOMMO_TEST_FOLDER", "tls/not-there.key");
  EXPECT_TRUE(std::filesystem::path(key).is_absolute()) << key;
  EXPECT_TRUE(key.ends_with("/tls/not-there.key")) << key;
}

}  // namespace
