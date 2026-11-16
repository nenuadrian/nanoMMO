#ifndef NANOMMO_SERVER_INPUTS_H_
#define NANOMMO_SERVER_INPUTS_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>

#include "absl/status/status.h"
#include "protocol/nanommo.pb.h"
#include "server/world.h"

namespace nanommo {

/// What each player's inputs do to the world, whoever runs it: a world server,
/// or a client that plays on its own. A numbered input's keys wait for their
/// tick, so that the client knows which tick each of its inputs moved it in.
class Inputs {
 public:
  /// A client sends an input a tick, so a few queue up only when the network
  /// holds some back and lets them through together. More than this, and the
  /// oldest go, so that a player doesn't fall further and further behind.
  static constexpr std::size_t kMostQueued = 5;

  /// A client sends one a tick, but a slow frame sends two or three together,
  /// so up to three waiting is a client a little ahead of our tick. More means
  /// some came late and we're behind it, so each tick skips one, until we've
  /// caught up.
  static constexpr std::size_t kMostBehind = 3;

  /// Inputs come in as fast as they go out, so a queue a burst filled stays
  /// that full, each input a tick late for each one ahead of it. If two or
  /// more waited at the start of each tick for a second, the queue's kept more
  /// than it needs, and it drops one, until one waiting is the most it keeps
  /// for long.
  static constexpr std::size_t kMostSteady = 2;
  static constexpr int kSteadyTicks = 20;

  /// A numbered input says when, on our clock, its client drew everyone else,
  /// and its player's blows on a duel opponent are checked against where the
  /// opponent was then. A client draws them 100 milliseconds before the newest
  /// world it has, and the world's way to it and the input's way back are a
  /// round trip more, so this is enough for a round trip of up to 100. A time
  /// further back is taken as this, so that a client can't hit its opponent
  /// wherever they once were, and a time ahead of ours as now.
  static constexpr std::chrono::milliseconds kMostRewind{200};

  /// The input's keys: a numbered one's wait for their tick, and others are
  /// set at once, unless the player numbers theirs, when one that isn't is a
  /// copy of the last, sent with something else.
  void Take(World& land, uint32_t id, const protocol::Input& input);

  /// Each player's next waiting keys, a tick's worth, in their land.
  void Apply(const std::function<World*(uint32_t)>& land_of);

  /// The last of the player's numbered inputs applied, if any.
  std::optional<uint32_t> acked(uint32_t id) const;

  /// Forgets a player who's left.
  void Leave(uint32_t id);

 private:
  /// The fewest of a player's inputs waiting at the start of a tick, over the
  /// ticks since their queue was last checked.
  struct Steady {
    std::size_t fewest = std::numeric_limits<std::size_t>::max();
    int ticks = 0;
  };

  std::map<uint32_t, std::deque<protocol::Input>> queued_;
  std::map<uint32_t, uint32_t> acked_;
  std::map<uint32_t, Steady> steady_;
};

/// Moves, uses or throws away an item, as the action asks, or says why not,
/// in words for the player.
absl::Status DoItem(World& land, uint32_t id,
                    const protocol::ItemAction& action);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_INPUTS_H_
