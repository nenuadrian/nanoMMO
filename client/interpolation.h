#ifndef NANOMMO_CLIENT_INTERPOLATION_H_
#define NANOMMO_CLIENT_INTERPOLATION_H_

#include <cstdint>
#include <deque>
#include <map>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Everyone but you, drawn a little in the past, between the two worlds from
/// the server on either side of that moment, so that they move smoothly
/// whenever the worlds come, as long as each next one has come by then.
class Interpolation {
 public:
  /// How far behind the server's clock we draw them: two of its ticks, so a
  /// world can come up to one tick late and still be there in time.
  static constexpr double kDelay = 0.1;

  /// A world as it came, at the time on our clock, in seconds. More of a world
  /// we have, as its other parts come, fills it in, and keeps the time its
  /// first part came.
  void Add(const protocol::World& world, double now);

  /// The newest world, with the players other than you, and the monsters,
  /// where they were kDelay ago on the server's clock.
  protocol::World Shown(const protocol::World& newest, double now) const;

 private:
  struct Place {
    float x = 0.0f;
    float z = 0.0f;
  };
  struct Snapshot {
    /// On the server's clock, in seconds.
    double sent = 0.0;
    /// How far our clock was ahead of the server's when it came.
    double ahead = 0.0;
    std::map<uint32_t, Place> players;
    std::map<uint32_t, Place> monsters;
  };
  /// The time on the server's clock to draw at, now.
  double Then(double now) const;

  std::deque<Snapshot> snapshots_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_INTERPOLATION_H_
