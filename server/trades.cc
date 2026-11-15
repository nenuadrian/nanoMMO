#include "server/trades.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

#include "google/protobuf/util/message_differencer.h"
#include "protocol/places.h"

namespace nanommo {

namespace {

// Whether what's there is the item that was offered, with the same rolls,
// however many of it there are.
bool SameItem(const items::Stack* there, const items::Stack& offered) {
  if (there == nullptr) {
    return false;
  }
  items::Stack same = *there;
  same.set_count(offered.count());
  return google::protobuf::util::MessageDifferencer::Equals(same, offered);
}

}  // namespace

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

uint32_t Trades::RevisionOf(uint32_t id) const {
  const auto side = sides_.find(id);
  return side != sides_.end() ? side->second.revision : 0;
}

bool Trades::Holds(uint32_t id, const protocol::Place& place) const {
  const auto side = sides_.find(id);
  return side != sides_.end() && place.has_bag() &&
         side->second.offer.bag().contains(place.bag());
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
  // There's something new to look at, so neither side is ready, and a ready
  // sent for what was there before is for the revision before.
  const uint32_t other = side->second.with;
  sides_.at(other).offer.set_ready(false);
  sides_.at(other).revision = ++side->second.revision;
  news_.insert({id, other});
  return absl::OkStatus();
}

absl::StatusOr<bool> Trades::Ready(uint32_t id, bool ready, uint32_t revision) {
  const auto side = sides_.find(id);
  if (side == sides_.end()) {
    return absl::FailedPreconditionError("You're not trading with anyone.");
  }
  // An offer can change while a ready's on its way, and the ready is for the
  // offers the player saw.
  if (ready && revision != side->second.revision) {
    return absl::FailedPreconditionError(
        "The trade has changed, so look at it again.");
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
  // for what it gets. Each place has to have the item still, and at least as
  // many as were offered, or Take fails: a kill can add to a stack that's
  // offered, and what it adds stays the player's.
  for (std::size_t side = 0; side < 2; ++side) {
    for (const auto& [slot, stack] : offers[side]->bag()) {
      if (!SameItem(after[side].At(InBag(slot)), stack) ||
          !after[side].Take(InBag(slot), stack.count()).ok()) {
        return refuse(absl::FailedPreconditionError(
            "The items offered aren't all there any more."));
      }
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
