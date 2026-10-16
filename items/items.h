#ifndef NANOMMO_ITEMS_ITEMS_H_
#define NANOMMO_ITEMS_ITEMS_H_

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "items/items.pb.h"

namespace nanommo {

/// The items in the items file, by id.
struct ItemTable {
  std::map<uint32_t, items::Item> by_id;
  uint32_t starting_weapon = 0;
};

/// The items in text format, or what's wrong with them: a field items.proto
/// doesn't have, an item with no id or with another's, or a starting weapon
/// that isn't one.
absl::StatusOr<ItemTable> ParseItems(std::string_view text);

absl::StatusOr<ItemTable> ReadItems(const std::string& path);

}  // namespace nanommo

#endif  // NANOMMO_ITEMS_ITEMS_H_
