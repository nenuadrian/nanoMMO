#ifndef NANOMMO_SERVER_TRADES_H_
#define NANOMMO_SERVER_TRADES_H_

#include <cstdint>
#include <map>
#include <set>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/items.h"
#include "protocol/nanommo.pb.h"
#include "server/inventory.h"

namespace nanommo {

/// The trades between the players in one copy of a land, which its World
/// keeps, so that both players are always in it. A player asks another to
/// trade, and the trade opens once the other asks them back. Each side offers
/// stacks from their bag, by their places, and gold, and once both sides are
/// ready, each gets what the other offered.
///
/// This is the simple version. An offer keeps a copy of each stack as it was
/// offered, and the trade gives those copies, and takes as many as were
/// offered off each place, trusting that what was offered is still there. The
/// client doesn't move what it's offered.
class Trades {
 public:
  /// The player asks the other to trade, in place of anyone they asked before.
  /// If the other asked them first, the trade between them opens, and it
  /// returns true. Neither may be trading already.
  bool Ask(uint32_t id, uint32_t other);

  /// Who the player's trading with, by their id, or 0 for nobody.
  uint32_t With(uint32_t id) const;

  /// What the player offers, and whether they're ready, while they trade.
  const protocol::TradeSide& SideOf(uint32_t id) const;

  /// What the player offers from their bag, in place of what they offered
  /// before: copies of the stacks at the offer's places, whole, and gold they
  /// have. Neither side is ready any more, as there's something new to look
  /// at. Or the error says why not, in words for the player.
  absl::Status Offer(uint32_t id, const Inventory& bag, const ItemTable& items,
                     const protocol::TradeOffer& offer);

  /// Whether the player's ready to trade what's offered, both ways. Returns
  /// whether both sides are now, or the error if they aren't trading.
  absl::StatusOr<bool> Ready(uint32_t id, bool ready);

  /// Once both sides are ready: each bag gives up what it offered and gets
  /// what the other offered, all of it or none, and the trade's over. If a
  /// bag hasn't room, or a side hasn't the gold it offered, neither side is
  /// ready any more, and the error says why, in words for both players.
  absl::Status Complete(uint32_t id, Inventory& bag, Inventory& other_bag,
                        const ItemTable& items);

  /// Ends the player's trade, if they're in one, their asking, and anyone's
  /// asking them. Returns who they were trading with, or 0.
  uint32_t End(uint32_t id);

  /// The players whose trade has opened, changed or ended since the last
  /// time, which it hands over.
  std::set<uint32_t> TakeNews();

 private:
  /// One player's side of a trade: who with, by their id, and their offer.
  struct Side {
    uint32_t with = 0;
    protocol::TradeSide offer;
  };

  /// Who each player has asked to trade, by their ids.
  std::map<uint32_t, uint32_t> asked_;
  /// Both sides of each trade, by each player's id.
  std::map<uint32_t, Side> sides_;
  std::set<uint32_t> news_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TRADES_H_
