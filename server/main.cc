#include <chrono>
#include <iostream>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "server/tick.h"

int main() {
  constexpr int kTicksPerSecond = 20;
  const absl::StatusOr<std::chrono::nanoseconds> interval =
      nanommo::TickInterval(kTicksPerSecond);
  if (!interval.ok()) {
    std::cerr << "nanommo server: " << interval.status() << "\n";
    return 1;
  }

  std::cout << "nanommo server starting: " << kTicksPerSecond << " ticks/s, "
            << absl::FormatDuration(absl::FromChrono(*interval))
            << " per tick\n";
  return 0;
}
