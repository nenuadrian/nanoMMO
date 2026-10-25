#ifndef NANOMMO_SERVER_INVENTORY_H_
#define NANOMMO_SERVER_INVENTORY_H_

#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "content/items.h"
#include "protocol/nanommo.pb.h"
#include "protocol/places.h"

namespace nanommo {

/// What a player carries, in a bag of kBagSlots, and wears, an item in each
/// slot, kept as the message that's saved and sent. Each change takes it to
/// its next revision. Items the table doesn't have, such as one taken out of
/// the content, stay where they are, so that nothing is lost by a mistake in
/// the file, but they can't be worn, used or stacked.
class Inventory {
 public:
  static constexpr uint32_t kBagSlots = nanommo::kBagSlots;

  Inventory() = default;
  explicit Inventory(protocol::Inventory items) : items_(std::move(items)) {}

  const protocol::Inventory& items() const { return items_; }
  uint32_t revision() const { return items_.revision(); }

  /// What's at the place, or nullptr when it's empty, or isn't a place.
  const items::Stack* At(const protocol::Place& place) const;

  /// Into the bag: onto stacks of the item that have room, then into empty
  /// slots, from the first. Returns how many didn't fit.
  uint32_t Add(const ItemTable& table, const items::Stack& stack);

  /// Wears it, if it's worn and its slot is free, and returns whether it did,
  /// as for a new player's kit.
  bool Wear(const ItemTable& table, const items::Stack& stack);

  /// Moves count of what's at from to to, or all of it given zero: into an
  /// empty place, onto a stack of the same item, as many as it has room for,
  /// or swapping with what's there. What's worn has to be worn in its slot.
  /// Or the error says why not, in words for the player.
  absl::Status Move(const ItemTable& table, const protocol::Place& from,
                    const protocol::Place& to, uint32_t count = 0);

  /// Takes one off the stack at the place, as using one does.
  absl::Status TakeOne(const protocol::Place& place);

  /// Throws away all of what's at the place.
  absl::Status Destroy(const protocol::Place& place);

 private:
  items::Stack* MutableAt(const protocol::Place& place);
  void Put(const protocol::Place& place, items::Stack stack);
  void Clear(const protocol::Place& place);
  void Changed() { items_.set_revision(items_.revision() + 1); }

  protocol::Inventory items_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_INVENTORY_H_
