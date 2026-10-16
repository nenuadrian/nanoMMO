#include "content/content.h"

#include "gtest/gtest.h"

TEST(Content, ReadsTheGamesContent) {
  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(NANOMMO_CONTENT);
  ASSERT_TRUE(content.ok()) << content.status();

  const nanommo::items::Item& sword =
      content->items.by_id.at(content->items.starting_weapon);
  EXPECT_EQ(sword.name(), "Sword");
  EXPECT_GT(sword.weapon().damage(), 0u);
  EXPECT_FALSE(sword.look().model().empty());
}

TEST(Content, RefusesItemsThatDontAddUp) {
  EXPECT_EQ(nanommo::ParseItems(
                "starting_weapon: 1 items { id: 1 weapon { reech: 2 } }")
                .status()
                .message(),
            "line 1: Message type \"nanommo.items.Weapon\" has no field named "
            "\"reech\".");
  EXPECT_FALSE(nanommo::ParseItems("starting_weapon: 1 "
                                   "items { id: 1 weapon {} } "
                                   "items { id: 1 weapon {} }")
                   .ok());
  EXPECT_FALSE(
      nanommo::ParseItems("starting_weapon: 2 items { id: 1 weapon {} }").ok());
}
