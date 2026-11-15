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
/// Nothing a client says is trusted. What's offered is held where it is until
/// the trade's over, each change to an offer takes the trade to its next
/// revision, which a ready has to name, and the trade gives nothing unless
/// each place still has what was offered.
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

  /// The revision of the player's trade, from 0 as it opens, which each change
  /// to either offer takes to the next.
  uint32_t RevisionOf(uint32_t id) const;

  /// Whether the place is one the player offers from, which stays as it is
  /// until the trade's over, as the trade takes what was offered from there.
  bool Holds(uint32_t id, const protocol::Place& place) const;

  /// What the player offers from their bag, in place of what they offered
  /// before: copies of the stacks at the offer's places, whole, and gold they
  /// have. Neither side is ready any more, as there's something new to look
  /// at, and the trade's at its next revision. Or the error says why not, in
  /// words for the player.
  absl::Status Offer(uint32_t id, const Inventory& bag, const ItemTable& items,
                     const protocol::TradeOffer& offer);

  /// Whether the player's ready to trade what's offered, both ways, at the
  /// revision they saw. Returns whether both sides are now, or the error if
  /// they aren't trading, or are ready for offers that have changed since.
  absl::StatusOr<bool> Ready(uint32_t id, bool ready, uint32_t revision);

  /// Once both sides are ready: each bag gives up what it offered and gets
  /// what the other offered, all of it or none, and the trade's over. Each
  /// place offered has to have at least as many as it did, of the same item
  /// with the same rolls. If one hasn't, a bag hasn't room, or a side hasn't
  /// the gold it offered, neither side is ready any more, and the error says
  /// why, in words for both players.
  absl::Status Complete(uint32_t id, Inventory& bag, Inventory& other_bag,
                        const ItemTable& items);

  /// Ends the player's trade, if they're in one, their asking, and anyone's
  /// asking them. Returns who they were trading with, or 0.
  uint32_t End(uint32_t id);

  /// The players whose trade has opened, changed or ended since the last
  /// time, which it hands over.
  std::set<uint32_t> TakeNews();

 private:
  /// One player's side of a trade: who with, by their id, their offer, and
  /// the trade's revision, the same on both sides.
  struct Side {
    uint32_t with = 0;
    protocol::TradeSide offer;
    uint32_t revision = 0;
  };

  /// Who each player has asked to trade, by their ids.
  std::map<uint32_t, uint32_t> asked_;
  /// Both sides of each trade, by each player's id.
  std::map<uint32_t, Side> sides_;
  std::set<uint32_t> news_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TRADES_H_
