#include "server/database.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

TEST(Database, RunsATransactionWholeOrNotAtAll) {
  const std::unique_ptr<nanommo::Database> database =
      nanommo::testing::TestDatabase();
  if (!database) {
    GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
  }
  const std::string name = nanommo::testing::TestName();
  const auto create = [&] {
    return database
        ->Run(
            "INSERT INTO accounts (username, password_hash) VALUES (?, 'hash')",
            {name})
        .status();
  };
  const auto accounts = [&] {
    return database->Run("SELECT id FROM accounts WHERE username = ?", {name})
        ->rows.size();
  };

  // The second statement fails, so the first doesn't happen either.
  EXPECT_FALSE(
      database
          ->Transaction([&] {
            const absl::Status created = create();
            if (!created.ok()) {
              return created;
            }
            return database->Run("INSERT INTO nowhere VALUES (1)").status();
          })
          .ok());
  EXPECT_EQ(accounts(), 0u);

  EXPECT_TRUE(database->Transaction(create).ok());
  EXPECT_EQ(accounts(), 1u);
  EXPECT_TRUE(
      database->Run("DELETE FROM accounts WHERE username = ?", {name}).ok());
}
