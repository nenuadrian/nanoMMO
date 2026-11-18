#include "server/tick.h"

#include <chrono>

#include "absl/status/status.h"

namespace nanommo {

absl::StatusOr<std::chrono::nanoseconds> TickInterval(int ticks_per_second) {
  if (ticks_per_second <= 0) {
    return absl::InvalidArgumentError("ticks_per_second must be positive");
  }
  return std::chrono::nanoseconds(std::chrono::seconds(1)) / ticks_per_second;
}

std::chrono::steady_clock::time_point NextTick(
    std::chrono::steady_clock::time_point due,
    std::chrono::steady_clock::time_point now,
    std::chrono::nanoseconds interval) {
  const std::chrono::steady_clock::time_point next = due + interval;
  if (now - next > kMostBehind) {
    return now + interval;
  }
  return next;
}

}  // namespace nanommo
