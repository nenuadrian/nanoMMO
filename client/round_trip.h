#ifndef NANOMMO_CLIENT_ROUND_TRIP_H_
#define NANOMMO_CLIENT_ROUND_TRIP_H_

#include <cstdint>
#include <map>

namespace nanommo {

/// How long the server takes to apply an input of ours and say so: from
/// sending each numbered input to the world that says it was applied.
class RoundTrip {
 public:
  /// We sent the input of that sequence, at the time, in seconds.
  void Sent(uint32_t sequence, double now);

  /// The world says the server applied the inputs up to acked.
  void Acked(uint32_t acked, double now);

  /// In milliseconds, smoothed over the last few, or 0 before any.
  float milliseconds() const { return milliseconds_; }

 private:
  std::map<uint32_t, double> sent_;
  float milliseconds_ = 0.0f;
};

/// The round trip, in the top right, at the top given, right-aligned, under
/// the minimap. Returns how tall it is, for what goes under it.
float DrawRoundTrip(float milliseconds, float top);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ROUND_TRIP_H_
