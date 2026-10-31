#include "server/events.h"

#include <optional>
#include <string>

#include "absl/time/time.h"
#include "content/content.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "protocol/nanommo.pb.h"
#include "server/world.h"

TEST(Events, MakesARowForClickHouse) {
  const nanommo::Event event{.kind = nanommo::Event::Kind::kLoot,
                             .player = "alice",
                             .land = 1,
                             .x = 1.5f,
                             .z = -2.0f,
                             .monster = 2,
                             .item = 7,
                             .count = 1};
  const nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  const std::string row = nanommo::Row(event, content, "Ember",
                                       absl::FromUnixMillis(1'793'318'400'123));

  // A line of its own, as JSONEachRow reads a row a line.
  EXPECT_EQ(row.find('\n'), std::string::npos);
  const nlohmann::json json = nlohmann::json::parse(row);
  EXPECT_EQ(json["time"], "2026-10-30 00:00:00.123");
  EXPECT_EQ(json["world"], "Ember");
  EXPECT_EQ(json["land"], "Ashlands");
  EXPECT_EQ(json["kind"], "loot");
  // By name, from the content, so that queries read it.
  EXPECT_EQ(json["what"], "Ash Buckler");
  EXPECT_EQ(json["player"], "alice");
  EXPECT_EQ(json["x"], 1.5);
  EXPECT_EQ(json["monster"], 2);
  EXPECT_EQ(json["item"], 7);
  EXPECT_EQ(json["count"], 1);
  EXPECT_EQ(json["quest"], 0);
}

TEST(Events, KeepsTheKillsForTheAnalyticsAndNotTheBlows) {
  EXPECT_TRUE(nanommo::ForAnalytics(nanommo::Event::Kind::kKill));
  EXPECT_TRUE(nanommo::ForAnalytics(nanommo::Event::Kind::kUse));
  EXPECT_FALSE(nanommo::ForAnalytics(nanommo::Event::Kind::kHit));
  EXPECT_FALSE(nanommo::ForAnalytics(nanommo::Event::Kind::kHeal));
}

TEST(Events, ShowsThePlayersTheBlowsAndNotTheirQuests) {
  const std::optional<nanommo::protocol::Event> hit =
      nanommo::ForPlayers({.kind = nanommo::Event::Kind::kHit,
                           .player_id = 3,
                           .monster_id = 9,
                           .amount = 15});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->kind(), nanommo::protocol::EVENT_HIT);
  EXPECT_EQ(hit->player(), 3u);
  EXPECT_EQ(hit->monster(), 9u);
  EXPECT_EQ(hit->amount(), 15u);

  // A level's amount is the level reached.
  const std::optional<nanommo::protocol::Event> level = nanommo::ForPlayers(
      {.kind = nanommo::Event::Kind::kLevel, .player_id = 3, .level = 7});
  ASSERT_TRUE(level.has_value());
  EXPECT_EQ(level->kind(), nanommo::protocol::EVENT_LEVEL);
  EXPECT_EQ(level->amount(), 7u);

  EXPECT_FALSE(
      nanommo::ForPlayers({.kind = nanommo::Event::Kind::kTake, .player_id = 3})
          .has_value());
}
