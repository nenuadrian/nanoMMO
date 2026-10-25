#include "bot/bag.h"

#include <cstdint>
#include <optional>

#include "protocol/places.h"

namespace nanommo {

std::optional<protocol::ItemAction> Tend(const ItemTable& items,
                                         const protocol::Inventory& bag,
                                         int health, int max_health,
                                         bool can_drink) {
  const bool hurt = health > 0 && health * 2 < max_health;
  for (uint32_t slot = 0; slot < kBagSlots; ++slot) {
    const auto stack = bag.bag().find(slot);
    const auto item = stack != bag.bag().end()
                          ? items.by_id.find(stack->second.item())
                          : items.by_id.end();
    if (item == items.by_id.end()) {
      continue;
    }
    protocol::ItemAction action;
    if (hurt && can_drink && item->second.use().heal() > 0) {
      action.set_use(slot);
      return action;
    }
    const items::Slot worn = item->second.equip().slot();
    if (!hurt && item->second.has_equip() && !bag.worn().contains(worn)) {
      *action.mutable_move()->mutable_from() = InBag(slot);
      *action.mutable_move()->mutable_to() = Worn(worn);
      return action;
    }
  }
  return std::nullopt;
}

}  // namespace nanommo
