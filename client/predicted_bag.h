#ifndef NANOMMO_CLIENT_PREDICTED_BAG_H_
#define NANOMMO_CLIENT_PREDICTED_BAG_H_

#include <cstdint>
#include <optional>

#include "content/items.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The bag a step ahead of the server: what the player just did with an item,
/// done at once to a copy of the bag, by the rules the server keeps, so that
/// an item lands the moment it's dropped, rather than a round trip later. Once
/// the server's bag comes at a new revision, it's shown instead; if it doesn't
/// come in time, as when the server refused, the copy goes, and the item's
/// back where the server has it.
class PredictedBag {
 public:
  /// In seconds.
  static constexpr float kPatience = 1.0f;

  explicit PredictedBag(const ItemTable& items) : items_(items) {}

  /// The bag to show, with the frame's seconds gone by: ours while we're
  /// ahead, or the server's.
  protocol::Inventory Shown(const protocol::Inventory& server, float seconds);

  /// What the player asked for, done to the bag shown, if the rules allow it.
  void Act(const protocol::Inventory& server,
           const protocol::ItemAction& action);

  bool ahead() const { return ahead_.has_value(); }

 private:
  const ItemTable& items_;
  std::optional<protocol::Inventory> ahead_;
  // The server's revision when we went ahead, and how long we've waited.
  uint32_t revision_ = 0;
  float waited_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PREDICTED_BAG_H_
