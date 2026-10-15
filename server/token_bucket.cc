#include "server/token_bucket.h"

#include <algorithm>
#include <chrono>

namespace nanommo {

TokenBucket::TokenBucket(int size, std::chrono::steady_clock::duration refill)
    : size_(size), refill_(refill), tokens_(size) {}

bool TokenBucket::Take(std::chrono::steady_clock::time_point now) {
  const double refilled =
      (now - last_) / std::chrono::duration<double>(refill_);
  tokens_ = std::min(tokens_ + refilled, static_cast<double>(size_));
  last_ = now;
  if (tokens_ < 1.0) {
    return false;
  }
  tokens_ -= 1.0;
  return true;
}

}  // namespace nanommo
