#ifndef NANOMMO_BOT_BAG_H_
#define NANOMMO_BOT_BAG_H_

#include <optional>

#include "content/items.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What a bot does with its bag, if anything: drinks something that heals,
/// once it's down to half its health and can drink again, or else wears
/// something that's worn in a slot it has free.
std::optional<protocol::ItemAction> Tend(const ItemTable& items,
                                         const protocol::Inventory& bag,
                                         int health, int max_health,
                                         bool can_drink);

}  // namespace nanommo

#endif  // NANOMMO_BOT_BAG_H_
