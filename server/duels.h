#ifndef NANOMMO_SERVER_DUELS_H_
#define NANOMMO_SERVER_DUELS_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "terrain/lands.h"

namespace nanommo {

/// The duels between the players in one copy of a land, which its World
/// keeps, as it does their trades, so that both players are always in it. A
/// player asks another to duel, and once the other says yes, a countdown
/// starts, and then the duel. A player is in one duel at a time, from the ask
/// to the end.
class Duels {
 public:
  /// A player asks another within this many metres of them.
  static constexpr float kReach = 10.0f;

  /// An ask waits this long for a yes.
  static constexpr std::chrono::seconds kAskTime{30};

  /// From the yes to the duel, so that both sides are ready for it.
  static constexpr std::chrono::seconds kCountdown{3};

  /// The flag goes down where the one who asked stands as the other says
  /// yes, and a duellist who goes further from it than this loses.
  static constexpr float kFlagReach = 25.0f;

  enum class State { kAsked, kCountdown, kFighting };

  struct Duel {
    /// Who asked, and who they asked, by their ids.
    uint32_t challenger = 0;
    uint32_t challenged = 0;
    State state = State::kAsked;
    /// How long the ask has left, and then the countdown.
    std::chrono::nanoseconds left = kAskTime;
    Place flag;

    /// The other side from the player's, by their id.
    uint32_t Against(uint32_t id) const {
      return id == challenger ? challenged : challenger;
    }
  };

  /// The player asks the other to duel. Neither may be in a duel already.
  void Ask(uint32_t id, uint32_t other);

  /// The player says yes to whoever asked them, which puts the flag down and
  /// starts the countdown. Returns who asked, or 0 if nobody did.
  uint32_t Accept(uint32_t id, Place flag);

  /// The player's duel, from the ask to the end, or nullptr.
  const Duel* Of(uint32_t id) const;

  /// Who the player's duelling, by their id, once the countdown's over, or 0.
  uint32_t Opponent(uint32_t id) const;

  /// Ends the player's duel, however far it's got. Returns who it was with,
  /// or 0.
  uint32_t End(uint32_t id);

  /// Runs the asks and the countdowns down, and starts the duels whose
  /// countdowns are over. Returns the asks that ran out, which are over.
  std::vector<Duel> Step(std::chrono::nanoseconds time);

  /// Each duel, by its challenger's id.
  const std::map<uint32_t, Duel>& all() const { return duels_; }

  /// The players whose duel has been asked for, counted down, started or
  /// ended since the last time, which it hands over.
  std::set<uint32_t> TakeNews();

 private:
  std::map<uint32_t, Duel> duels_;
  /// The challenger of each player's duel, by the ids of both sides.
  std::map<uint32_t, uint32_t> challengers_;
  std::set<uint32_t> news_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_DUELS_H_
