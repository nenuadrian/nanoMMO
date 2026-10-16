#include "content/items.h"

#include <cstdint>
#include <map>
#include <string_view>
#include <utility>

#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<ItemTable> ParseItems(std::string_view text) {
  items::Items file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, items::Item>> by_id = ById(file.items());
  if (!by_id.ok()) {
    return by_id.status();
  }
  ItemTable table{.by_id = *std::move(by_id),
                  .starting_weapon = file.starting_weapon()};
  const auto starting = table.by_id.find(table.starting_weapon);
  if (starting == table.by_id.end() || !starting->second.has_weapon()) {
    return absl::InvalidArgumentError("the starting weapon isn't a weapon");
  }
  return table;
}

}  // namespace nanommo
