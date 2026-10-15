#ifndef NANOMMO_SERVER_TOKEN_BUCKET_H_
#define NANOMMO_SERVER_TOKEN_BUCKET_H_

#include <chrono>

namespace nanommo {

/// Lets up to `size` through at once, and then one each `refill`, like a
/// bucket of tokens that fills up again over time, where each one let through
/// takes a token out.
class TokenBucket {
 public:
  TokenBucket(int size, std::chrono::steady_clock::duration refill);

  /// Takes a token out, if the bucket has one at this time.
  bool Take(std::chrono::steady_clock::time_point now);

 private:
  int size_;
  std::chrono::steady_clock::duration refill_;
  double tokens_;
  std::chrono::steady_clock::time_point last_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TOKEN_BUCKET_H_
