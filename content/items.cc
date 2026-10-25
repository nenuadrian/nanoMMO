#include "content/items.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

namespace {

// What one item's parts can't be together, or what's wrong with one of them.
absl::Status CheckParts(const items::Item& item) {
  const std::string& name = item.name();
  if (item.has_weapon() && item.equip().slot() != items::SLOT_MAIN_HAND) {
    return absl::InvalidArgumentError(absl::StrCat(
        "the ", name, " is a weapon, so it's held in the main hand"));
  }
  if (item.has_equip() && item.equip().slot() == items::SLOT_NONE) {
    return absl::InvalidArgumentError(
        absl::StrCat("the ", name, " is worn, but has no slot to be worn in"));
  }
  // Worn items, and items that roll, are each one of a kind, which a stack of
  // them can't be.
  if (StackSize(item) > 1 && (item.has_equip() || !item.rolls().empty())) {
    return absl::InvalidArgumentError(absl::StrCat(
        "the ", name, " stacks, so it can't be worn, or roll its own"));
  }
  for (const items::Modifier& modifier : item.equip().modifiers()) {
    if (modifier.stat() == items::STAT_NONE) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", name, " adds to no stat"));
    }
  }
  for (const items::Roll& roll : item.rolls()) {
    if (roll.stat() == items::STAT_NONE || roll.min() > roll.max()) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", name, " rolls from ", roll.min(), " to ",
                       roll.max(), " on ", items::Stat_Name(roll.stat())));
    }
  }
  return absl::OkStatus();
}

}  // namespace

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
  for (const auto& [id, item] : *by_id) {
    const absl::Status checked = CheckParts(item);
    if (!checked.ok()) {
      return checked;
    }
    if (std::ranges::find(file.retired(), id) != file.retired().end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "the ", item.name(), " has the id ", id, ", which is retired"));
    }
  }
  ItemTable table{.by_id = *std::move(by_id),
                  .starting = {file.starting().begin(), file.starting().end()}};
  for (const items::Stack& stack : table.starting) {
    const auto item = table.by_id.find(stack.item());
    if (item == table.by_id.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no item ", stack.item(), " to start with"));
    }
    if (CountOf(stack) > StackSize(item->second)) {
      return absl::InvalidArgumentError(
          absl::StrCat("a new player starts with ", CountOf(stack), " of the ",
                       item->second.name(), ", which don't fit in a slot"));
    }
  }
  return table;
}

uint32_t StackSize(const items::Item& item) {
  return std::max(item.stack(), 1u);
}

uint32_t CountOf(const items::Stack& stack) {
  return std::max(stack.count(), 1u);
}

std::string_view SlotName(items::Slot slot) {
  switch (slot) {
    case items::SLOT_MAIN_HAND:
      return "main hand";
    case items::SLOT_OFF_HAND:
      return "off hand";
    case items::SLOT_HEAD:
      return "head";
    case items::SLOT_BODY:
      return "body";
    case items::SLOT_FEET:
      return "feet";
    default:
      return "nowhere";
  }
}

}  // namespace nanommo
