#ifndef NANOMMO_CHEAT_MEASURE_H_
#define NANOMMO_CHEAT_MEASURE_H_

#include <deque>

namespace nanommo {

/// How fast the server moves a player, from where its worlds put them: what a
/// client that breaks the rules gets for it, if anything.
class Speedometer {
 public:
  /// Where a world had the player, sent at the time, in seconds on the
  /// server's clock. A world older than the newest it has is left out.
  void Add(double sent, float x, float z);

  /// In metres a second, along the way it went over the last second of
  /// worlds, so that turning back doesn't count against it, or 0 before two.
  float MetresASecond() const;

 private:
  struct Place {
    double sent = 0.0;
    float x = 0.0f;
    float z = 0.0f;
  };
  std::deque<Place> places_;
};

}  // namespace nanommo

#endif  // NANOMMO_CHEAT_MEASURE_H_
