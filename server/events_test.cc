#include "server/events.h"

#include <string>

#include "absl/time/time.h"
#include "content/content.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
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
