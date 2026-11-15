#ifndef NANOMMO_CLIENT_TRADE_WINDOW_H_
#define NANOMMO_CLIENT_TRADE_WINDOW_H_

#include <cstdint>
#include <optional>
#include <utility>

#include "content/items.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What a side of a trade offers, as an offer that asks for it again: its
/// places in the bag, in order, and its gold.
protocol::TradeOffer OfferOf(const protocol::TradeSide& side);

/// A trade with another player, in a window of its own, which is open while
/// the trade is. What you offer is on the left: drag items onto it from the
/// bag, click one to take it back, and type the gold. What they offer is on
/// the right. A side's slots light green once it's ready, Ready says you are,
/// and a change to either offer makes neither side ready. Closing the window
/// calls the trade off.
class TradeWindow {
 public:
  explicit TradeWindow(ItemTable items) : items_(std::move(items)) {}

  /// Draws it while there's a trade. Returns what the player does, which the
  /// server does, or says in the chat why not.
  std::optional<protocol::TradeAction> Build(const protocol::Trade& trade);

 private:
  ItemTable items_;
  /// The gold being typed, which is the offer's while nobody's typing.
  uint32_t gold_ = 0;
  bool typing_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TRADE_WINDOW_H_
