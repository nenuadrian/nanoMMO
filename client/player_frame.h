#ifndef NANOMMO_CLIENT_PLAYER_FRAME_H_
#define NANOMMO_CLIENT_PLAYER_FRAME_H_

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "content/factions.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// You, in the top left: your face in a ring, your name and level, your health
/// and how far you are into your level, and the faction you've sworn to.
class PlayerFrame {
 public:
  explicit PlayerFrame(std::map<uint32_t, factions::Faction> factions)
      : factions_(std::move(factions)) {}

  /// The portrait's size, in pixels, twice what it's shown at on most
  /// screens, so that it's sharp on the rest.
  static constexpr int kPortraitPixels = 192;

  /// Draws your head and shoulders into the portrait, before the frame, as
  /// you look now.
  void Render(const Renderer& renderer);

  void Draw(const std::vector<Figure>& figures,
            const protocol::World& world) const;

 private:
  std::map<uint32_t, factions::Faction> factions_;
  Offscreen portrait_{kPortraitPixels, kPortraitPixels};
  bool rendered_ = false;
};

/// The monster you picked, by its id, or in a duel, the player, by theirs:
/// its name, level and health, at the top in the middle, under the compass,
/// while there's one.
void DrawTargetFrame(const std::vector<Figure>& figures, uint32_t target,
                     uint32_t target_player = 0);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PLAYER_FRAME_H_
