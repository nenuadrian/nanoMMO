#include "server/consul.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "server/test_services.h"

using nanommo::protocol::WorldListing;

TEST(ConsulTest, ListsAWorldUntilItDeregisters) {
  const char* host = std::getenv("NANOMMO_CONSUL_HOST");
  if (host == nullptr) {
    GTEST_SKIP() << "set NANOMMO_CONSUL_HOST to test against Consul";
  }
  nanommo::Consul consul(host);
  const std::string name = nanommo::testing::TestName();
  ASSERT_TRUE(consul
                  .Register({.name = name,
                             .public_host = "example.test",
                             .port = 7000,
                             .metrics_port = 9000,
                             .players = 12})
                  .ok());

  const absl::StatusOr<std::vector<WorldListing>> listed = consul.Worlds();
  ASSERT_TRUE(listed.ok()) << listed.status();
  const auto world = std::ranges::find(*listed, name, &WorldListing::name);
  ASSERT_NE(world, listed->end());
  EXPECT_EQ(world->host(), "example.test");
  EXPECT_EQ(world->port(), 7000u);
  EXPECT_EQ(world->players(), 12u);

  ASSERT_TRUE(consul.Deregister(name).ok());
  const absl::StatusOr<std::vector<WorldListing>> left = consul.Worlds();
  ASSERT_TRUE(left.ok()) << left.status();
  EXPECT_EQ(std::ranges::find(*left, name, &WorldListing::name), left->end());
}
