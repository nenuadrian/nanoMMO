#include "items/items.h"

#include "gtest/gtest.h"

TEST(Items, ReadsTheGamesItems) {
  const absl::StatusOr<nanommo::ItemTable> items =
      nanommo::ReadItems(NANOMMO_ITEMS);
  ASSERT_TRUE(items.ok()) << items.status();

  const nanommo::items::Item& sword = items->by_id.at(items->starting_weapon);
  EXPECT_EQ(sword.name(), "Sword");
  EXPECT_GT(sword.weapon().damage(), 0u);
  EXPECT_FALSE(sword.look().model().empty());
}

TEST(Items, RefusesItemsThatDontAddUp) {
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
