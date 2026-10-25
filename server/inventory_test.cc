#include "server/inventory.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "content/items.h"
#include "gtest/gtest.h"
#include "server/test_content.h"

namespace {

constexpr uint32_t kSword = 1;
constexpr uint32_t kPotion = 2;
constexpr uint32_t kBone = 3;
constexpr uint32_t kCap = 4;
constexpr uint32_t kBlade = 5;

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

// What's at the place, as (item, count), or (0, 0) for nothing.
std::pair<uint32_t, uint32_t> Holds(const nanommo::Inventory& inventory,
                                    const nanommo::protocol::Place& place) {
  const nanommo::items::Stack* stack = inventory.At(place);
  return stack != nullptr ? std::pair(stack->item(), stack->count())
                          : std::pair(0u, 0u);
}

// How many of each item, wherever they are.
std::map<uint32_t, uint32_t> Counts(const nanommo::Inventory& inventory) {
  std::map<uint32_t, uint32_t> counts;
  for (const auto& [slot, stack] : inventory.items().bag()) {
    counts[stack.item()] += stack.count();
  }
  for (const auto& [slot, stack] : inventory.items().worn()) {
    counts[stack.item()] += stack.count();
  }
  return counts;
}

}  // namespace

TEST(Inventory, AddsOntoStacksThenIntoEmptySlots) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;

  EXPECT_EQ(inventory.Add(gear, Of(kBone, 30)), 0u);
  EXPECT_EQ(inventory.Add(gear, Of(kPotion, 3)), 0u);
  EXPECT_EQ(inventory.Add(gear, Of(kBone, 25)), 0u);

  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kBone, 50u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(1)), std::pair(kPotion, 3u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(2)), std::pair(kBone, 5u));
}

TEST(Inventory, SaysHowManyDidntFit) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  for (uint32_t slot = 0; slot < nanommo::Inventory::kBagSlots - 1; ++slot) {
    ASSERT_EQ(inventory.Add(gear, Of(kCap)), 0u);
  }
  ASSERT_EQ(inventory.Add(gear, Of(kBone, 45)), 0u);

  // The last slot has room for five more bones, and caps don't stack.
  EXPECT_EQ(inventory.Add(gear, Of(kBone, 8)), 3u);
  EXPECT_EQ(inventory.Add(gear, Of(kCap)), 1u);
  // Nor does an item the content doesn't have go anywhere.
  EXPECT_EQ(inventory.Add(gear, Of(99, 2)), 2u);
  EXPECT_EQ(Counts(inventory).at(kBone), 50u);
}

TEST(Inventory, SplitsAndMergesStacks) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  inventory.Add(gear, Of(kBone, 30));

  EXPECT_TRUE(
      inventory.Move(gear, nanommo::InBag(0), nanommo::InBag(5), 10).ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kBone, 20u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(5)), std::pair(kBone, 10u));

  // Back onto the first, all of them, which leaves the slot empty.
  EXPECT_TRUE(inventory.Move(gear, nanommo::InBag(5), nanommo::InBag(0)).ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kBone, 30u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(5)), std::pair(0u, 0u));

  // Onto a stack with room for twenty, of thirty.
  inventory.Add(gear, Of(kBone, 50));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kBone, 50u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(1)), std::pair(kBone, 30u));
  EXPECT_EQ(
      inventory.Move(gear, nanommo::InBag(1), nanommo::InBag(0)).message(),
      "That stack is full.");
}

TEST(Inventory, WearsWhatsWornInItsSlot) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  inventory.Add(gear, Of(kCap));
  inventory.Add(gear, Of(kBone, 4));

  EXPECT_EQ(inventory
                .Move(gear, nanommo::InBag(0),
                      nanommo::Worn(nanommo::items::SLOT_MAIN_HAND))
                .message(),
            "The Cap goes in the head slot.");
  EXPECT_EQ(inventory
                .Move(gear, nanommo::InBag(1),
                      nanommo::Worn(nanommo::items::SLOT_HEAD))
                .message(),
            "You can't wear the Bone.");
  EXPECT_TRUE(inventory
                  .Move(gear, nanommo::InBag(0),
                        nanommo::Worn(nanommo::items::SLOT_HEAD))
                  .ok());
  EXPECT_EQ(Holds(inventory, nanommo::Worn(nanommo::items::SLOT_HEAD)),
            std::pair(kCap, 1u));

  // Taking the cap off onto the bones would put them on the head, and an empty
  // slot is fine.
  EXPECT_EQ(inventory
                .Move(gear, nanommo::Worn(nanommo::items::SLOT_HEAD),
                      nanommo::InBag(1))
                .message(),
            "You can't wear the Bone.");
  EXPECT_TRUE(inventory
                  .Move(gear, nanommo::Worn(nanommo::items::SLOT_HEAD),
                        nanommo::InBag(7))
                  .ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(7)), std::pair(kCap, 1u));
}

TEST(Inventory, SwapsWhatsWornForWhatsInTheBag) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  ASSERT_TRUE(inventory.Wear(gear, Of(kSword)));
  EXPECT_FALSE(inventory.Wear(gear, Of(kBlade)));
  EXPECT_FALSE(inventory.Wear(gear, Of(kBone)));
  inventory.Add(gear, Of(kBlade));

  const nanommo::protocol::Place hand =
      nanommo::Worn(nanommo::items::SLOT_MAIN_HAND);
  EXPECT_TRUE(inventory.Move(gear, nanommo::InBag(0), hand).ok());
  EXPECT_EQ(Holds(inventory, hand), std::pair(kBlade, 1u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kSword, 1u));

  // Only all of a stack swaps.
  inventory.Add(gear, Of(kBone, 10));
  inventory.Add(gear, Of(kPotion, 2));
  EXPECT_EQ(
      inventory.Move(gear, nanommo::InBag(1), nanommo::InBag(2), 4).message(),
      "Only a whole stack can swap places.");
  EXPECT_TRUE(inventory.Move(gear, nanommo::InBag(1), nanommo::InBag(2)).ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(1)), std::pair(kPotion, 2u));
  EXPECT_EQ(Holds(inventory, nanommo::InBag(2)), std::pair(kBone, 10u));
}

TEST(Inventory, RefusesPlacesThatArentThere) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  inventory.Add(gear, Of(kBone));

  EXPECT_EQ(inventory
                .Move(gear, nanommo::InBag(0),
                      nanommo::InBag(nanommo::Inventory::kBagSlots))
                .message(),
            "There's no such place.");
  EXPECT_EQ(inventory.Move(gear, nanommo::InBag(0), {}).message(),
            "There's no such place.");
  EXPECT_EQ(
      inventory.Move(gear, nanommo::InBag(3), nanommo::InBag(4)).message(),
      "There's nothing there.");
  EXPECT_TRUE(inventory
                  .Move(gear, nanommo::InBag(0),
                        nanommo::InBag(nanommo::Inventory::kBagSlots - 1))
                  .ok());
}

TEST(Inventory, TakesOneAtATimeAndThrowsAway) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  inventory.Add(gear, Of(kPotion, 2));
  inventory.Add(gear, Of(kBone, 9));

  EXPECT_TRUE(inventory.TakeOne(nanommo::InBag(0)).ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(kPotion, 1u));
  EXPECT_TRUE(inventory.TakeOne(nanommo::InBag(0)).ok());
  EXPECT_EQ(Holds(inventory, nanommo::InBag(0)), std::pair(0u, 0u));
  EXPECT_FALSE(inventory.TakeOne(nanommo::InBag(0)).ok());

  EXPECT_TRUE(inventory.Destroy(nanommo::InBag(1)).ok());
  EXPECT_TRUE(Counts(inventory).empty());
  EXPECT_FALSE(inventory.Destroy(nanommo::InBag(1)).ok());
}

TEST(Inventory, CountsEachChangeAsARevision) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory inventory;
  EXPECT_EQ(inventory.revision(), 0u);

  inventory.Add(gear, Of(kBone, 5));
  EXPECT_EQ(inventory.revision(), 1u);
  ASSERT_TRUE(inventory.Move(gear, nanommo::InBag(0), nanommo::InBag(1)).ok());
  EXPECT_EQ(inventory.revision(), 2u);

  // What changes nothing isn't a change: a refusal, a move to where it is, or
  // an item that doesn't fit.
  ASSERT_FALSE(inventory.Move(gear, nanommo::InBag(0), nanommo::InBag(1)).ok());
  ASSERT_TRUE(inventory.Move(gear, nanommo::InBag(1), nanommo::InBag(1)).ok());
  inventory.Add(gear, Of(99));
  EXPECT_EQ(inventory.revision(), 2u);
}

TEST(Inventory, KeepsItemsTheContentDoesntHave) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::protocol::Inventory saved;
  (*saved.mutable_bag())[0] = Of(99);
  saved.set_revision(4);
  nanommo::Inventory inventory(saved);

  EXPECT_TRUE(inventory.Move(gear, nanommo::InBag(0), nanommo::InBag(1)).ok());
  EXPECT_EQ(inventory
                .Move(gear, nanommo::InBag(1),
                      nanommo::Worn(nanommo::items::SLOT_HEAD))
                .message(),
            "You can't wear that.");
  EXPECT_EQ(Holds(inventory, nanommo::InBag(1)), std::pair(99u, 1u));
  EXPECT_EQ(inventory.revision(), 5u);
}

// Whatever a player does with their bag, moving things around makes no items
// and loses none, no stack holds more than it can, and what's worn is worn in
// its slot.
TEST(Inventory, NeverMakesOrLosesItemsAsTheyMove) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  std::mt19937 random(7);
  const auto any_place = [&random] {
    if (std::bernoulli_distribution(0.2)(random)) {
      return nanommo::Worn(static_cast<nanommo::items::Slot>(
          std::uniform_int_distribution(0, 6)(random)));
    }
    return nanommo::InBag(std::uniform_int_distribution(
        0u, nanommo::Inventory::kBagSlots)(random));
  };
  // Mostly from somewhere with something in it, so that most moves move.
  const auto any_stack = [&random,
                          &any_place](const nanommo::Inventory& inventory) {
    std::vector<nanommo::protocol::Place> full;
    for (const auto& [slot, stack] : inventory.items().bag()) {
      full.push_back(nanommo::InBag(slot));
    }
    for (const auto& [slot, stack] : inventory.items().worn()) {
      full.push_back(nanommo::Worn(static_cast<nanommo::items::Slot>(slot)));
    }
    if (full.empty() || std::bernoulli_distribution(0.1)(random)) {
      return any_place();
    }
    return full[std::uniform_int_distribution<std::size_t>(
        0, full.size() - 1)(random)];
  };
  const auto stacks = [](const nanommo::Inventory& inventory) {
    return inventory.items().bag_size() + inventory.items().worn_size();
  };
  nanommo::Inventory inventory;
  std::map<uint32_t, uint32_t> expected;
  // So that the moves did split stacks, merge them, and wear things.
  int splits = 0;
  int merges = 0;
  bool wore = false;
  for (int step = 0; step < 20000; ++step) {
    // Now and then something new comes in, and something goes, so that the bag
    // never stays full.
    if (step % 20 == 0) {
      const uint32_t item = std::uniform_int_distribution(1u, 5u)(random);
      const uint32_t most =
          nanommo::StackSize(gear.by_id.at(item)) > 1 ? 60 : 1;
      const uint32_t count = std::uniform_int_distribution(1u, most)(random);
      const uint32_t left = inventory.Add(gear, Of(item, count));
      ASSERT_LE(left, count);
      if (count > left) {
        expected[item] += count - left;
      }
    }
    if (step % 15 == 0) {
      const nanommo::protocol::Place place = any_stack(inventory);
      if (const nanommo::items::Stack* stack = inventory.At(place)) {
        const uint32_t item = stack->item();
        expected[item] -= stack->count();
        if (expected[item] == 0) {
          expected.erase(item);
        }
        ASSERT_TRUE(inventory.Destroy(place).ok());
      }
    }
    const uint32_t before = inventory.revision();
    const int stacks_before = stacks(inventory);
    const bool moved = inventory
                           .Move(gear, any_stack(inventory), any_place(),
                                 std::uniform_int_distribution(0u, 20u)(random))
                           .ok();
    ASSERT_GE(inventory.revision(), before);
    ASSERT_TRUE(moved || inventory.revision() == before);
    splits += stacks(inventory) > stacks_before;
    merges += stacks(inventory) < stacks_before;

    ASSERT_EQ(Counts(inventory), expected) << "at step " << step;
    for (const auto& [slot, stack] : inventory.items().bag()) {
      ASSERT_LT(slot, nanommo::Inventory::kBagSlots);
      ASSERT_GE(stack.count(), 1u);
      ASSERT_LE(stack.count(), nanommo::StackSize(gear.by_id.at(stack.item())));
    }
    for (const auto& [slot, stack] : inventory.items().worn()) {
      ASSERT_EQ(stack.count(), 1u);
      ASSERT_EQ(gear.by_id.at(stack.item()).equip().slot(), slot);
      wore = true;
    }
  }
  EXPECT_GT(splits, 100);
  EXPECT_GT(merges, 50);
  EXPECT_TRUE(wore);
}
