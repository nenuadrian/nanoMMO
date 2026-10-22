#include "server/consul.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "server/test_services.h"

using nanommo::protocol::WorldListing;

TEST(ConsulTest, ListsEachWorldsNewestInstanceUntilItDeregisters) {
  const char* host = std::getenv("NANOMMO_CONSUL_HOST");
  if (host == nullptr) {
    GTEST_SKIP() << "set NANOMMO_CONSUL_HOST to test against Consul";
  }
  nanommo::Consul consul(host);
  const std::string name = nanommo::testing::TestName();
  const nanommo::WorldRegistration older = {.name = name,
                                            .public_host = "example.test",
                                            .port = 7000,
                                            .metrics_port = 9000,
                                            .players = 12,
                                            .content = 0xfedcba9876543210,
                                            .started = 100};
  nanommo::WorldRegistration newer = older;
  newer.port = 7010;
  newer.players = 0;
  newer.started = 200;
  ASSERT_TRUE(consul.Register(older).ok());
  ASSERT_TRUE(consul.Register(newer).ok());

  // Both instances are up, and the world is listed once, at the newer one.
  const absl::StatusOr<std::vector<WorldListing>> listed = consul.Worlds();
  ASSERT_TRUE(listed.ok()) << listed.status();
  EXPECT_EQ(std::ranges::count(*listed, name, &WorldListing::name), 1);
  const auto world = std::ranges::find(*listed, name, &WorldListing::name);
  ASSERT_NE(world, listed->end());
  EXPECT_EQ(world->host(), "example.test");
  EXPECT_EQ(world->port(), 7010u);
  EXPECT_EQ(world->players(), 0u);
  EXPECT_EQ(world->content(), 0xfedcba9876543210u);

  ASSERT_TRUE(consul.Deregister(nanommo::InstanceId(newer)).ok());
  const absl::StatusOr<std::vector<WorldListing>> older_left = consul.Worlds();
  ASSERT_TRUE(older_left.ok()) << older_left.status();
  const auto back = std::ranges::find(*older_left, name, &WorldListing::name);
  ASSERT_NE(back, older_left->end());
  EXPECT_EQ(back->port(), 7000u);

  ASSERT_TRUE(consul.Deregister(nanommo::InstanceId(older)).ok());
  const absl::StatusOr<std::vector<WorldListing>> left = consul.Worlds();
  ASSERT_TRUE(left.ok()) << left.status();
  EXPECT_EQ(std::ranges::find(*left, name, &WorldListing::name), left->end());
}
