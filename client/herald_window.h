#ifndef NANOMMO_CLIENT_HERALD_WINDOW_H_
#define NANOMMO_CLIENT_HERALD_WINDOW_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "content/factions.h"

namespace nanommo {

/// A herald's window, which opens when the player clicks one: the herald's
/// faction, in its colour, and Enlist, while you're sworn to none, which asks
/// once more before you swear, as it's for good. Once you're sworn, to theirs
/// or another, it says to which.
class HeraldWindow {
 public:
  explicit HeraldWindow(std::map<uint32_t, factions::Faction> factions)
      : factions_(std::move(factions)) {}

  /// The herald, by the NPC's id, and their faction, by its id.
  void Open(uint32_t npc, std::string name, uint32_t faction);

  bool open() const { return npc_.has_value(); }
  void Close();

  /// Draws the window, while it's open, for you, among the figures. Returns
  /// the herald to swear at, by the NPC's id, once you've said you're sure,
  /// and closes.
  std::optional<uint32_t> Build(const std::vector<Figure>& figures);

 private:
  std::map<uint32_t, factions::Faction> factions_;
  std::optional<uint32_t> npc_;
  std::string name_;
  uint32_t faction_ = 0;
  /// Whether Enlist was clicked, and it's asking whether you're sure.
  bool asking_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_HERALD_WINDOW_H_
