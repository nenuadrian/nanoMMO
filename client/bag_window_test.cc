#include "client/bag_window.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "content/content.h"
#include "gtest/gtest.h"
#include "protocol/places.h"

namespace {

// The game's own items, as the client reads them.
nanommo::ItemTable Items() {
  return nanommo::ReadContent(NANOMMO_CONTENT)->items;
}

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

constexpr uint32_t kSword = 1;
constexpr uint32_t kPotion = 2;
constexpr uint32_t kTunic = 5;
constexpr uint32_t kBlade = 6;

}  // namespace

TEST(BagWindow, SaysWhatAnItemIsAndWhatItAdds) {
  nanommo::items::Stack blade = Of(kBlade);
  nanommo::items::Modifier* roll = blade.add_rolls();
  roll->set_stat(nanommo::items::STAT_DAMAGE);
  roll->set_amount(3);
  EXPECT_EQ(nanommo::ItemLines(Items(), blade),
            (std::vector<std::string>{"Ash Blade", "Worn: main hand",
                                      "+3 damage, rolled"}));
  EXPECT_EQ(nanommo::ItemLines(Items(), Of(kTunic)),
            (std::vector<std::string>{"Leather Tunic", "Worn: body",
                                      "+1 armour", "+10 health"}));
  EXPECT_EQ(nanommo::ItemLines(Items(), Of(kPotion, 3)),
            (std::vector<std::string>{"Health Potion",
                                      "Use: heals 40, one every 10 s",
                                      "Stacks up to 10"}));
  EXPECT_EQ(nanommo::ItemLines(Items(), Of(99)),
            (std::vector<std::string>{"An item this client doesn't know"}));
}

TEST(BagWindow, DoubleClickUsesWearsOrTakesOff) {
  nanommo::protocol::Inventory bag;
  (*bag.mutable_bag())[0] = Of(kPotion, 3);
  (*bag.mutable_bag())[1] = Of(kTunic);
  (*bag.mutable_worn())[nanommo::items::SLOT_MAIN_HAND] = Of(kSword);

  const std::optional<nanommo::protocol::ItemAction> drink =
      nanommo::QuickAction(Items(), bag, nanommo::InBag(0));
  ASSERT_TRUE(drink && drink->has_use());
  EXPECT_EQ(drink->use(), 0u);

  const std::optional<nanommo::protocol::ItemAction> wear =
      nanommo::QuickAction(Items(), bag, nanommo::InBag(1));
  ASSERT_TRUE(wear && wear->has_move());
  EXPECT_EQ(wear->move().to().worn(), nanommo::items::SLOT_BODY);

  // Off, into the first slot with nothing in it.
  const std::optional<nanommo::protocol::ItemAction> off = nanommo::QuickAction(
      Items(), bag, nanommo::Worn(nanommo::items::SLOT_MAIN_HAND));
  ASSERT_TRUE(off && off->has_move());
  EXPECT_EQ(off->move().to().bag(), 2u);
  EXPECT_FALSE(nanommo::QuickAction(Items(), bag, nanommo::InBag(5)));
}

TEST(BagWindow, FindsTheFirstEmptySlot) {
  nanommo::protocol::Inventory bag;
  EXPECT_EQ(nanommo::FirstEmptySlot(bag), 0u);
  for (uint32_t slot = 0; slot < nanommo::kBagSlots; ++slot) {
    if (slot != 7) {
      (*bag.mutable_bag())[slot] = Of(kPotion);
    }
  }
  EXPECT_EQ(nanommo::FirstEmptySlot(bag), 7u);
  (*bag.mutable_bag())[7] = Of(kPotion);
  EXPECT_EQ(nanommo::FirstEmptySlot(bag), std::nullopt);
}
