#ifndef NANOMMO_PROTOCOL_CLOCK_H_
#define NANOMMO_PROTOCOL_CLOCK_H_

#include <chrono>
#include <cstdint>

namespace nanommo {

/// The system clock, not the steady one, so that a client can compare it
/// with its own, as long as their clocks agree.
inline int64_t NowMicros() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_CLOCK_H_
