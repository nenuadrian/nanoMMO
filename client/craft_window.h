#ifndef NANOMMO_CLIENT_CRAFT_WINDOW_H_
#define NANOMMO_CLIENT_CRAFT_WINDOW_H_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// How many of the item are in the bag, in all its slots, and not worn, as a
/// recipe takes from the bag alone.
uint32_t HowMany(const protocol::Inventory& bag, uint32_t item);

/// Whether a player of the level can start making the recipe, as the server
/// would let them: it's of their level or below, and the bag has all it
/// takes. The server checks for room, and for what's offered in a trade.
bool CanMake(const recipes::Recipe& recipe, const protocol::Inventory& bag,
             int level);

/// How far what's being made has got, from 0 to 1, from how long it's been
/// at, in milliseconds, of its recipe's seconds.
float MadeShare(const recipes::Recipe& recipe, uint32_t done);

/// The recipes the player knows, which are those of their level or below, in
/// a window of its own, which K opens and closes: what each takes, as how many
/// they have of how many it needs, what it makes, and a button to make it,
/// which is off while they can't. While they're making something, a bar over
/// the hotbar shows how far it's got, open or not.
class CraftWindow {
 public:
  explicit CraftWindow(Content content) : content_(std::move(content)) {}

  bool visible() const { return visible_; }
  void Toggle() { visible_ = !visible_; }

  /// Draws the bar, while the player's making something, and the window,
  /// while it's open, with their bag. Returns what they ask for: to start
  /// making a recipe, or to stop.
  std::optional<protocol::CraftAction> Build(const std::vector<Figure>& figures,
                                             const protocol::Inventory& bag,
                                             const protocol::Crafting& making);

 private:
  void DrawProgress(const protocol::Crafting& making) const;

  Content content_;
  bool visible_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CRAFT_WINDOW_H_
