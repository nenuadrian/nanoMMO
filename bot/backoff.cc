#include "bot/backoff.h"

#include <algorithm>
#include <chrono>
#include <random>

namespace nanommo {

std::chrono::milliseconds Backoff(int failed, std::mt19937& random) {
  constexpr std::chrono::milliseconds kFirst{1000};
  constexpr std::chrono::milliseconds kMost{30000};
  // Past five doublings it's at its most already, and a shift that far
  // would overflow.
  const std::chrono::milliseconds wait =
      std::min(kFirst * (1 << std::clamp(failed, 0, 5)), kMost);
  std::uniform_int_distribution<std::chrono::milliseconds::rep> part(
      wait.count() / 2, wait.count());
  return std::chrono::milliseconds(part(random));
}

}  // namespace nanommo
