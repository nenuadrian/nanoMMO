#ifndef NANOMMO_SERVER_HISTORY_H_
#define NANOMMO_SERVER_HISTORY_H_

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>

#include "terrain/lands.h"

namespace nanommo {

/// Where each player has been, a place a tick, for the last second, which a
/// World keeps. A client draws everyone else a moment in the past, so a blow
/// it aimed at a player can be checked against where that player was then,
/// not where they are now.
class History {
 public:
  /// A second: more than any blow goes back.
  static constexpr std::chrono::seconds kKept{1};

  /// The player is at the place at the time, on the world's clock, which only
  /// goes on. Their places from more than kKept before it go.
  void Record(uint32_t id, std::chrono::nanoseconds time, Place place);

  /// Where the player was at the time, between their places on either side of
  /// it. Before their oldest, it's that, and after their newest, that. Or
  /// nothing, for a player it has no places of.
  std::optional<Place> At(uint32_t id, std::chrono::nanoseconds time) const;

  /// Forgets a player who's left.
  void Leave(uint32_t id);

 private:
  struct Moment {
    std::chrono::nanoseconds time{0};
    Place place;
  };
  std::map<uint32_t, std::deque<Moment>> moments_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_HISTORY_H_
