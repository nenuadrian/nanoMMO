#ifndef NANOMMO_SERVER_TEST_SERVICES_H_
#define NANOMMO_SERVER_TEST_SERVICES_H_

#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/database.h"
#include "server/redis.h"

namespace nanommo::testing {

/// MySQL at NANOMMO_MYSQL_HOST, or nullptr if that isn't set. If it's set and
/// we can't connect, the test fails.
inline std::unique_ptr<Database> TestDatabase() {
  const char* host = std::getenv("NANOMMO_MYSQL_HOST");
  if (host == nullptr) {
    return nullptr;
  }
  absl::StatusOr<std::unique_ptr<Database>> database =
      Database::Create(host, "nanommo", "nanommo", "nanommo");
  if (!database.ok()) {
    ADD_FAILURE() << database.status();
    return nullptr;
  }
  return *std::move(database);
}

/// Redis at NANOMMO_REDIS_HOST, like TestDatabase.
inline std::unique_ptr<Redis> TestRedis() {
  const char* host = std::getenv("NANOMMO_REDIS_HOST");
  if (host == nullptr) {
    return nullptr;
  }
  absl::StatusOr<std::unique_ptr<Redis>> redis = Redis::Create(host);
  if (!redis.ok()) {
    ADD_FAILURE() << redis.status();
    return nullptr;
  }
  return *std::move(redis);
}

inline std::string TestName() {
  return "test_" + std::to_string(std::random_device()() % 1000000000);
}

}  // namespace nanommo::testing

#endif  // NANOMMO_SERVER_TEST_SERVICES_H_
