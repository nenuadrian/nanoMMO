#ifndef NANOMMO_SERVER_TICK_H_
#define NANOMMO_SERVER_TICK_H_

#include <chrono>

#include "absl/status/statusor.h"

namespace nanommo {

/// Returns the time between two simulation ticks, accurate to the nanosecond,
/// when the world is updated `ticks_per_second` times a second. Returns an
/// InvalidArgument error if `ticks_per_second` isn't positive.
absl::StatusOr<std::chrono::nanoseconds> TickInterval(int ticks_per_second);

/// The most a world catches up on, by running the ticks it's behind back to
/// back.
inline constexpr std::chrono::seconds kMostBehind{1};

/// When the tick after the one that was due at `due` is due, once that one's
/// over, at `now`: an interval after it, so that the ticks don't drift, and a
/// tick that ran late is caught up on. A world that's more than kMostBehind
/// behind, as when it waited for Redis, doesn't run the ticks it missed back
/// to back, which would play the stall over at many times the speed: those
/// are gone, and the next is an interval from now.
std::chrono::steady_clock::time_point NextTick(
    std::chrono::steady_clock::time_point due,
    std::chrono::steady_clock::time_point now,
    std::chrono::nanoseconds interval);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TICK_H_
