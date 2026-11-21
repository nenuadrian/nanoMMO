#include "client/craft_window.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace {

constexpr uint32_t kSword = 1;
constexpr uint32_t kBone = 3;

// The game's: the sword and ten bones bind a Bone Sword, in eight seconds,
// from level 2.
nanommo::recipes::Recipe Bind() {
  return nanommo::ParseRecipes(R"pb(
           recipes {
             id: 2
             name: "Bind a Bone Sword"
             inputs { item: 1 count: 1 }
             inputs { item: 3 count: 10 }
             output { item: 13 count: 1 }
             seconds: 8
             level: 2
           }
         )pb")
      ->at(2);
}

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

}  // namespace

TEST(CraftWindow, CountsWhatsInTheBagAndNotWhatsWorn) {
  nanommo::protocol::Inventory bag;
  (*bag.mutable_bag())[0] = Of(kBone, 6);
  (*bag.mutable_bag())[3] = Of(kBone, 5);
  (*bag.mutable_worn())[nanommo::items::SLOT_MAIN_HAND] = Of(kSword);
  EXPECT_EQ(nanommo::HowMany(bag, kBone), 11u);
  EXPECT_EQ(nanommo::HowMany(bag, kSword), 0u);
}

TEST(CraftWindow, MakesWhatTheirLevelAndBagAllow) {
  nanommo::protocol::Inventory bag;
  (*bag.mutable_bag())[0] = Of(kBone, 6);
  (*bag.mutable_bag())[3] = Of(kBone, 5);
  (*bag.mutable_worn())[nanommo::items::SLOT_MAIN_HAND] = Of(kSword);
  // The sword in hand doesn't count, and one in the bag does.
  EXPECT_FALSE(nanommo::CanMake(Bind(), bag, 2));
  (*bag.mutable_bag())[5] = Of(kSword);
  EXPECT_TRUE(nanommo::CanMake(Bind(), bag, 2));
  EXPECT_FALSE(nanommo::CanMake(Bind(), bag, 1));
  (*bag.mutable_bag())[3] = Of(kBone, 3);
  EXPECT_FALSE(nanommo::CanMake(Bind(), bag, 2));
}

TEST(CraftWindow, FillsTheBarAsItsMade) {
  EXPECT_FLOAT_EQ(nanommo::MadeShare(Bind(), 0), 0.0f);
  EXPECT_FLOAT_EQ(nanommo::MadeShare(Bind(), 4000), 0.5f);
  EXPECT_FLOAT_EQ(nanommo::MadeShare(Bind(), 9000), 1.0f);
}
