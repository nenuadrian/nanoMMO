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

}  // namespace nanommo
