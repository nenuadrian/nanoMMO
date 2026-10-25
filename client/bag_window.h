#ifndef NANOMMO_CLIENT_BAG_WINDOW_H_
#define NANOMMO_CLIENT_BAG_WINDOW_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/items.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What a tooltip says of the stack: its item's name, where it's worn, what
/// it adds, what this one rolled, what using one does, and how many stack.
std::vector<std::string> ItemLines(const ItemTable& items,
                                   const items::Stack& stack);

/// The first slot of the bag with nothing in it, if it has one.
std::optional<uint32_t> FirstEmptySlot(const protocol::Inventory& bag);

/// What double-clicking the stack at the place does: in the bag, using it if
/// it's used, or else wearing it if it's worn, and taking it off if it's worn
/// now, into the first empty slot of the bag.
std::optional<protocol::ItemAction> QuickAction(const ItemTable& items,
                                                const protocol::Inventory& bag,
                                                const protocol::Place& place);

/// Draws a slot of the bag, or one that's worn, as a button of the size: the
/// item's icon in its tint, with how many there are. A player drags an item
/// from one slot onto another to move it, wear it or take it off, with Shift
/// for half the stack, double-clicks it to use, wear or take it off, and
/// right-clicks it for a menu that throws it away too. Returns what they ask
/// for, which the server does or refuses.
std::optional<protocol::ItemAction> ItemSlot(const ItemTable& items,
                                             const protocol::Inventory& bag,
                                             const protocol::Place& place,
                                             float size);

/// The bag, in a window of its own, which I or View, Bag opens and closes.
class BagWindow {
 public:
  explicit BagWindow(ItemTable items) : items_(std::move(items)) {}

  bool* mutable_visible() { return &visible_; }
  void Toggle() { visible_ = !visible_; }

  std::optional<protocol::ItemAction> Build(const protocol::Inventory& bag);

 private:
  ItemTable items_;
  bool visible_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_BAG_WINDOW_H_
