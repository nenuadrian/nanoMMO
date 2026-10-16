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
  const nanommo::monsters::Kind& skeleton = content->monsters.kinds.at(1);
  EXPECT_EQ(skeleton.name(), "Skeleton");
  EXPECT_FALSE(content->monsters.spawns.empty());
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

TEST(Content, RefusesMonstersThatDontAddUp) {
  EXPECT_TRUE(
      nanommo::ParseMonsters("kinds { id: 1 } "
                             "spawns { kind: 1 land: \"Meadows\" x: 5 }")
          .ok());
  EXPECT_FALSE(nanommo::ParseMonsters("kinds { id: 1 } "
                                      "spawns { kind: 2 land: \"Meadows\" }")
                   .ok());
  EXPECT_FALSE(nanommo::ParseMonsters("kinds { id: 1 } "
                                      "camps { kind: 1 land: \"Marsh\" }")
                   .ok());
  EXPECT_FALSE(
      nanommo::ParseMonsters("kinds { id: 1 fight { leash: 6 } } "
                             "spawns { kind: 1 land: \"Meadows\" x: 5 }")
          .ok());
}
