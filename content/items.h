#ifndef NANOMMO_CONTENT_ITEMS_H_
#define NANOMMO_CONTENT_ITEMS_H_

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "content/items.pb.h"

namespace nanommo {

/// The items in the items file, by id.
struct ItemTable {
  std::map<uint32_t, items::Item> by_id;
  /// What each new player starts with.
  std::vector<items::Stack> starting;
  /// The first weapon in the kit, which each player holds until they have a
  /// bag to keep the rest in.
  uint32_t starting_weapon = 0;
};

/// The items in text format, or what's wrong with them: a field items.proto
/// doesn't have, an item with no id or with another's, or with parts that
/// don't go together, such as a weapon that isn't held in the main hand, or a
/// kit with an item the file doesn't have.
absl::StatusOr<ItemTable> ParseItems(std::string_view text);

/// How many of the item go in one slot of a bag.
uint32_t StackSize(const items::Item& item);

/// How many are in the stack, which is one when it doesn't say.
uint32_t CountOf(const items::Stack& stack);

/// The slot's name, as a player would say it, such as "main hand".
std::string_view SlotName(items::Slot slot);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_ITEMS_H_
