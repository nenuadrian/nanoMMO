#include "server/trades.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

#include "protocol/places.h"

namespace nanommo {

bool Trades::Ask(uint32_t id, uint32_t other) {
  const auto asked = asked_.find(other);
  if (asked == asked_.end() || asked->second != id) {
    asked_[id] = other;
    return false;
  }
  asked_.erase(id);
  asked_.erase(other);
  sides_[id] = {.with = other};
  sides_[other] = {.with = id};
  news_.insert({id, other});
  return true;
}

uint32_t Trades::With(uint32_t id) const {
  const auto side = sides_.find(id);
  return side != sides_.end() ? side->second.with : 0;
}

const protocol::TradeSide& Trades::SideOf(uint32_t id) const {
  return sides_.at(id).offer;
}

absl::Status Trades::Offer(uint32_t id, const Inventory& bag,
                           const ItemTable& items,
                           const protocol::TradeOffer& offer) {
  const auto side = sides_.find(id);
  if (side == sides_.end()) {
    return absl::FailedPreconditionError("You're not trading with anyone.");
  }
  protocol::TradeSide offered;
  for (const uint32_t slot : offer.bag()) {
    const items::Stack* stack = bag.At(InBag(slot));
    if (stack == nullptr) {
      return absl::NotFoundError("There's nothing there.");
    }
    if (!items.by_id.contains(stack->item())) {
      return absl::FailedPreconditionError("You can't trade that.");
    }
    (*offered.mutable_bag())[slot] = *stack;
  }
  if (offer.gold() > bag.gold()) {
    return absl::FailedPreconditionError("You haven't that much gold.");
  }
  offered.set_gold(offer.gold());
  side->second.offer = std::move(offered);
  // There's something new to look at, so neither side is ready.
  const uint32_t other = side->second.with;
  sides_.at(other).offer.set_ready(false);
  news_.insert({id, other});
  return absl::OkStatus();
}

absl::StatusOr<bool> Trades::Ready(uint32_t id, bool ready) {
  const auto side = sides_.find(id);
  if (side == sides_.end()) {
    return absl::FailedPreconditionError("You're not trading with anyone.");
  }
  side->second.offer.set_ready(ready);
  const uint32_t other = side->second.with;
  news_.insert({id, other});
  return ready && SideOf(other).ready();
}

absl::Status Trades::Complete(uint32_t id, Inventory& bag, Inventory& other_bag,
                              const ItemTable& items) {
  const uint32_t other = With(id);
  const std::array offers = {&SideOf(id), &SideOf(other)};
  // Into copies, so that it's all of it or none.
  std::array<Inventory, 2> after = {bag, other_bag};
  const auto refuse = [&](const absl::Status& why) {
    sides_.at(id).offer.set_ready(false);
    sides_.at(other).offer.set_ready(false);
    news_.insert({id, other});
    return why;
  };
  // First each side gives up what it offered, so that its bag has that room
  // for what it gets. As many as were offered come off each place, which
  // trusts that what was offered is still there.
  for (std::size_t side = 0; side < 2; ++side) {
    for (const auto& [slot, stack] : offers[side]->bag()) {
      after[side].Take(InBag(slot), stack.count()).IgnoreError();
    }
    if (!after[side].TakeGold(offers[side]->gold()).ok()) {
      return refuse(absl::FailedPreconditionError(
          "The gold offered isn't all there any more."));
    }
  }
  // Then each gets the copies of what the other offered.
  for (std::size_t side = 0; side < 2; ++side) {
    Inventory& to = after[1 - side];
    for (const auto& [slot, stack] : offers[side]->bag()) {
      if (to.Add(items, stack) > 0) {
        return refuse(absl::ResourceExhaustedError(
            "There isn't room for the trade in one of the bags."));
      }
    }
    if (to.AddGold(offers[side]->gold()) > 0) {
      return refuse(absl::ResourceExhaustedError(
          "That's more gold than can be carried."));
    }
  }
  bag = std::move(after[0]);
  other_bag = std::move(after[1]);
  End(id);
  return absl::OkStatus();
}

uint32_t Trades::End(uint32_t id) {
  asked_.erase(id);
  std::erase_if(asked_, [id](const auto& asked) { return asked.second == id; });
  const auto side = sides_.find(id);
  if (side == sides_.end()) {
    return 0;
  }
  const uint32_t other = side->second.with;
  sides_.erase(side);
  sides_.erase(other);
  news_.insert({id, other});
  return other;
}

std::set<uint32_t> Trades::TakeNews() { return std::exchange(news_, {}); }

}  // namespace nanommo
