#include "server/inventory.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

bool IsPlace(const protocol::Place& place) {
  switch (place.where_case()) {
    case protocol::Place::kBag:
      return place.bag() < Inventory::kBagSlots;
    case protocol::Place::kWorn:
      return place.worn() != items::SLOT_NONE &&
             items::Slot_IsValid(place.worn());
    case protocol::Place::WHERE_NOT_SET:
      break;
  }
  return false;
}

// The bag's slots and the worn ones are each a map, by number.
uint32_t Key(const protocol::Place& place) {
  return place.has_bag() ? place.bag() : static_cast<uint32_t>(place.worn());
}

bool SamePlace(const protocol::Place& a, const protocol::Place& b) {
  return a.where_case() == b.where_case() && Key(a) == Key(b);
}

const items::Item* Find(const ItemTable& table, uint32_t id) {
  const auto found = table.by_id.find(id);
  return found != table.by_id.end() ? &found->second : nullptr;
}

uint32_t StackSizeOf(const ItemTable& table, uint32_t id) {
  const items::Item* item = Find(table, id);
  return item != nullptr ? StackSize(*item) : 1;
}

// Whether the stack can go at the place: anything goes in the bag, and a slot
// that's worn takes what's worn in it.
absl::Status CheckFits(const ItemTable& table, const items::Stack& stack,
                       const protocol::Place& place) {
  if (!place.has_worn()) {
    return absl::OkStatus();
  }
  const items::Item* item = Find(table, stack.item());
  if (item == nullptr || !item->has_equip()) {
    return absl::FailedPreconditionError(
        absl::StrCat("You can't wear ",
                     item != nullptr ? "the " + item->name() : "that", "."));
  }
  if (item->equip().slot() != place.worn()) {
    return absl::FailedPreconditionError(
        absl::StrCat("The ", item->name(), " goes in the ",
                     SlotName(item->equip().slot()), " slot."));
  }
  return absl::OkStatus();
}

}  // namespace

const items::Stack* Inventory::At(const protocol::Place& place) const {
  if (!IsPlace(place)) {
    return nullptr;
  }
  const auto& slots = place.has_bag() ? items_.bag() : items_.worn();
  const auto found = slots.find(Key(place));
  return found != slots.end() ? &found->second : nullptr;
}

items::Stack* Inventory::MutableAt(const protocol::Place& place) {
  if (!IsPlace(place)) {
    return nullptr;
  }
  auto& slots =
      place.has_bag() ? *items_.mutable_bag() : *items_.mutable_worn();
  const auto found = slots.find(Key(place));
  return found != slots.end() ? &found->second : nullptr;
}

void Inventory::Put(const protocol::Place& place, items::Stack stack) {
  auto& slots =
      place.has_bag() ? *items_.mutable_bag() : *items_.mutable_worn();
  slots[Key(place)] = std::move(stack);
}

void Inventory::Clear(const protocol::Place& place) {
  auto& slots =
      place.has_bag() ? *items_.mutable_bag() : *items_.mutable_worn();
  slots.erase(Key(place));
}

uint32_t Inventory::Add(const ItemTable& table, const items::Stack& stack) {
  uint32_t left = CountOf(stack);
  const items::Item* item = Find(table, stack.item());
  if (item == nullptr) {
    return left;
  }
  const uint32_t before = left;
  const uint32_t size = StackSize(*item);
  auto& bag = *items_.mutable_bag();
  if (size > 1) {
    for (uint32_t slot = 0; slot < kBagSlots && left > 0; ++slot) {
      const auto found = bag.find(slot);
      if (found != bag.end() && found->second.item() == stack.item() &&
          found->second.count() < size) {
        const uint32_t onto = std::min(left, size - found->second.count());
        found->second.set_count(found->second.count() + onto);
        left -= onto;
      }
    }
  }
  for (uint32_t slot = 0; slot < kBagSlots && left > 0; ++slot) {
    if (!bag.contains(slot)) {
      items::Stack put = stack;
      put.set_count(std::min(left, size));
      left -= put.count();
      bag[slot] = std::move(put);
    }
  }
  if (left < before) {
    Changed();
  }
  return left;
}

bool Inventory::Wear(const ItemTable& table, const items::Stack& stack) {
  const items::Item* item = Find(table, stack.item());
  if (item == nullptr || !item->has_equip() ||
      At(nanommo::Worn(item->equip().slot())) != nullptr) {
    return false;
  }
  items::Stack worn = stack;
  worn.set_count(1);
  Put(nanommo::Worn(item->equip().slot()), std::move(worn));
  Changed();
  return true;
}

absl::Status Inventory::Move(const ItemTable& table,
                             const protocol::Place& from,
                             const protocol::Place& to, uint32_t count) {
  if (!IsPlace(from) || !IsPlace(to)) {
    return absl::InvalidArgumentError("There's no such place.");
  }
  items::Stack* moving = MutableAt(from);
  if (moving == nullptr) {
    return absl::NotFoundError("There's nothing there.");
  }
  if (SamePlace(from, to)) {
    return absl::OkStatus();
  }
  if (const absl::Status fits = CheckFits(table, *moving, to); !fits.ok()) {
    return fits;
  }
  const uint32_t all = moving->count();
  const uint32_t moved = count == 0 ? all : std::min(count, all);
  items::Stack* there = MutableAt(to);
  if (there == nullptr) {
    items::Stack part = *moving;
    part.set_count(moved);
    moving->set_count(all - moved);
    if (moving->count() == 0) {
      Clear(from);
    }
    Put(to, std::move(part));
  } else if (there->item() == moving->item() &&
             StackSizeOf(table, there->item()) > 1) {
    const uint32_t size = StackSizeOf(table, there->item());
    const uint32_t room = size > there->count() ? size - there->count() : 0;
    if (room == 0) {
      return absl::FailedPreconditionError("That stack is full.");
    }
    const uint32_t onto = std::min(moved, room);
    there->set_count(there->count() + onto);
    moving->set_count(all - onto);
    if (moving->count() == 0) {
      Clear(from);
    }
  } else {
    if (moved < all) {
      return absl::FailedPreconditionError(
          "Only a whole stack can swap places.");
    }
    if (const absl::Status fits = CheckFits(table, *there, from); !fits.ok()) {
      return fits;
    }
    std::swap(*moving, *there);
  }
  Changed();
  return absl::OkStatus();
}

absl::Status Inventory::TakeOne(const protocol::Place& place) {
  items::Stack* stack = MutableAt(place);
  if (stack == nullptr) {
    return absl::NotFoundError("There's nothing there.");
  }
  if (stack->count() > 1) {
    stack->set_count(stack->count() - 1);
  } else {
    Clear(place);
  }
  Changed();
  return absl::OkStatus();
}

absl::Status Inventory::Destroy(const protocol::Place& place) {
  if (At(place) == nullptr) {
    return absl::NotFoundError("There's nothing there.");
  }
  Clear(place);
  Changed();
  return absl::OkStatus();
}

protocol::Place InBag(uint32_t slot) {
  protocol::Place place;
  place.set_bag(slot);
  return place;
}

protocol::Place Worn(items::Slot slot) {
  protocol::Place place;
  place.set_worn(slot);
  return place;
}

}  // namespace nanommo
