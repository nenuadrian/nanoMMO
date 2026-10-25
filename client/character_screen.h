#ifndef NANOMMO_CLIENT_CHARACTER_SCREEN_H_
#define NANOMMO_CLIENT_CHARACTER_SCREEN_H_

#include <optional>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "content/items.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

class CharacterScreen {
 public:
  explicit CharacterScreen(ItemTable items) : items_(std::move(items)) {}

  CharacterScreen(const CharacterScreen&) = delete;
  CharacterScreen& operator=(const CharacterScreen&) = delete;

  bool* mutable_visible() { return &visible_; }
  void Toggle() { visible_ = !visible_; }

  void Render(const Renderer& renderer);

  /// Draws the window, while it's open, with what the player wears, and
  /// returns what they do with it, as the bag's window does.
  std::optional<protocol::ItemAction> Build(const std::vector<Figure>& figures,
                                            const protocol::World& world,
                                            const char* land,
                                            const protocol::Inventory& bag);

 private:
  static constexpr int kPreviewPixels = 360;

  ItemTable items_;
  bool visible_ = false;
  Offscreen preview_{kPreviewPixels, kPreviewPixels};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTER_SCREEN_H_
