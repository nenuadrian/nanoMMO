#include "content/items.h"

#include <string_view>

#include "absl/strings/str_cat.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<ItemTable> ParseItems(std::string_view text) {
  items::Items file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  ItemTable table{.starting_weapon = file.starting_weapon()};
  for (const items::Item& item : file.items()) {
    if (item.id() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", item.name(), " has no id"));
    }
    if (!table.by_id.emplace(item.id(), item).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("two items have the id ", item.id()));
    }
  }
  const auto starting = table.by_id.find(table.starting_weapon);
  if (starting == table.by_id.end() || !starting->second.has_weapon()) {
    return absl::InvalidArgumentError("the starting weapon isn't a weapon");
  }
  return table;
}

}  // namespace nanommo
