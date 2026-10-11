#include "server/password.h"

#include <string>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"

TEST(Password, AcceptsTheRightPasswordOnly) {
  const absl::StatusOr<std::string> hash =
      nanommo::HashPassword("correct horse");
  ASSERT_TRUE(hash.ok());

  EXPECT_TRUE(nanommo::VerifyPassword(*hash, "correct horse"));
  EXPECT_FALSE(nanommo::VerifyPassword(*hash, "correct horsE"));
}

TEST(Password, SaltsEachHash) {
  EXPECT_NE(nanommo::HashPassword("correct horse"),
            nanommo::HashPassword("correct horse"));
}
