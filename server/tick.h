#ifndef NANOMMO_SERVER_TICK_H_
#define NANOMMO_SERVER_TICK_H_

#include <chrono>

#include "absl/status/statusor.h"

namespace nanommo {

/// Returns the time between two simulation ticks, accurate to the nanosecond,
/// when the world is updated `ticks_per_second` times a second. Returns an
/// InvalidArgument error if `ticks_per_second` isn't positive.
absl::StatusOr<std::chrono::nanoseconds> TickInterval(int ticks_per_second);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TICK_H_
