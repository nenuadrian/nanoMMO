#include "bot/bag.h"

#include <cstdint>
#include <optional>

#include "content/content.h"
#include "gtest/gtest.h"

namespace {

constexpr uint32_t kPotion = 2;
constexpr uint32_t kCap = 4;

nanommo::ItemTable Items() {
  return nanommo::ReadContent(NANOMMO_CONTENT)->items;
}

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

}  // namespace

TEST(Bag, DrinksOnceItsDownToHalfItsHealth) {
  nanommo::protocol::Inventory bag;
  (*bag.mutable_bag())[3] = Of(kPotion, 2);

  EXPECT_FALSE(nanommo::Tend(Items(), bag, 60, 100, true));
  const std::optional<nanommo::protocol::ItemAction> drink =
      nanommo::Tend(Items(), bag, 40, 100, true);
  ASSERT_TRUE(drink && drink->has_use());
  EXPECT_EQ(drink->use(), 3u);
  // Not while the last one's cooling down, nor once it's dead.
  EXPECT_FALSE(nanommo::Tend(Items(), bag, 40, 100, false));
  EXPECT_FALSE(nanommo::Tend(Items(), bag, 0, 100, true));
}

TEST(Bag, WearsWhatsWornInASlotItHasFree) {
  nanommo::protocol::Inventory bag;
  (*bag.mutable_bag())[5] = Of(kCap);

  const std::optional<nanommo::protocol::ItemAction> wear =
      nanommo::Tend(Items(), bag, 100, 100, true);
  ASSERT_TRUE(wear && wear->has_move());
  EXPECT_EQ(wear->move().from().bag(), 5u);
  EXPECT_EQ(wear->move().to().worn(), nanommo::items::SLOT_HEAD);

  (*bag.mutable_worn())[nanommo::items::SLOT_HEAD] = Of(kCap);
  EXPECT_FALSE(nanommo::Tend(Items(), bag, 100, 100, true));
}
