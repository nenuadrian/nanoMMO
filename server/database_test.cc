#include "server/database.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
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

TEST(Database, PreparesAReadOnceAndRunsItAgain) {
  const std::unique_ptr<nanommo::Database> database =
      nanommo::testing::TestDatabase();
  if (!database) {
    GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
  }
  // How many statements the connection has prepared, this one too.
  const auto prepared = [&] {
    const absl::StatusOr<nanommo::Result> shown =
        database->Run("SHOW SESSION STATUS LIKE 'Com_stmt_prepare'");
    int count = -1;
    if (shown.ok() && !shown->rows.empty()) {
      EXPECT_TRUE(absl::SimpleAtoi(shown->rows[0][1], &count));
    }
    return count;
  };
  const std::string name = nanommo::testing::TestName();
  const int before = prepared();
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(
        database->Run("SELECT id FROM accounts WHERE username = ?", {name})
            .ok());
  }
  // The read, once, and the SHOW that counts.
  EXPECT_EQ(prepared(), before + 2);
  // A write is prepared each time, as it was.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(
        database->Run("DELETE FROM accounts WHERE username = ?", {name}).ok());
  }
  EXPECT_EQ(prepared(), before + 2 + 4);
}

TEST(Database, RunsAReadAgainOnANewConnectionOnceItsOwnIsGone) {
  const std::unique_ptr<nanommo::Database> database =
      nanommo::testing::TestDatabase();
  const std::unique_ptr<nanommo::Database> other =
      nanommo::testing::TestDatabase();
  if (!database || !other) {
    GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
  }
  const absl::StatusOr<nanommo::Result> first =
      database->Run("SELECT CONNECTION_ID()");
  ASSERT_TRUE(first.ok()) << first.status();
  // As MySQL does to a connection that's sat idle for eight hours.
  ASSERT_TRUE(other->Run(absl::StrCat("KILL ", first->rows[0][0])).ok());

  const absl::StatusOr<nanommo::Result> again =
      database->Run("SELECT CONNECTION_ID()");
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_NE(again->rows, first->rows);
}

using std::chrono::seconds;
using std::chrono::steady_clock;

TEST(Database, AMysqlThatDoesntAnswerTimesOutConnecting) {
  const nanommo::testing::SilentListener silent;
  const steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<std::unique_ptr<nanommo::Database>> database =
      nanommo::Database::Create(absl::StrCat("127.0.0.1:", silent.port()),
                                "nanommo", "nanommo", "nanommo",
                                {.connect = seconds(1)});
  const steady_clock::duration took = steady_clock::now() - start;
  EXPECT_TRUE(absl::IsUnavailable(database.status())) << database.status();
  EXPECT_LT(took, seconds(4));
}

TEST(Database, AStatementMysqlDoesntAnswerInTimeFails) {
  const char* host = std::getenv("NANOMMO_MYSQL_HOST");
  if (host == nullptr) {
    GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
  }
  absl::StatusOr<std::unique_ptr<nanommo::Database>> database =
      nanommo::Database::Create(host, "nanommo", "nanommo", "nanommo",
                                {.answer = seconds(1)});
  ASSERT_TRUE(database.ok()) << database.status();
  // A statement that answers within the second does.
  ASSERT_TRUE((*database)->Run("SELECT SLEEP(0.1)").ok());

  // One that takes ten seconds, as MySQL hung would, fails, long before that.
  steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<nanommo::Result> slept =
      (*database)->Run("SELECT SLEEP(10)");
  const steady_clock::duration took = steady_clock::now() - start;
  EXPECT_TRUE(absl::IsUnavailable(slept.status())) << slept.status();
  EXPECT_GE(took, seconds(1));
  EXPECT_LT(took, seconds(5));

  // The next goes on a new connection, as the one that timed out is gone.
  const absl::StatusOr<nanommo::Result> again = (*database)->Run("SELECT 1");
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_EQ(again->rows, (std::vector<std::vector<std::string>>{{"1"}}));
}
