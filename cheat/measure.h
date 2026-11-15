#ifndef NANOMMO_CHEAT_MEASURE_H_
#define NANOMMO_CHEAT_MEASURE_H_

#include <cstdint>
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

/// The hits a player lands, and their damage, from the events the server
/// sends: what a client that breaks the rules gets for it in a fight.
class Hits {
 public:
  /// A hit that did the amount, at the time, in seconds on our clock.
  void Add(double at, uint32_t amount);

  /// Over the last five seconds to now, as a swing takes about one.
  float PerSecond(double now) const;
  float DamagePerSecond(double now) const;

 private:
  struct Hit {
    double at = 0.0;
    uint32_t amount = 0;
  };
  std::deque<Hit> hits_;
};

}  // namespace nanommo

#endif  // NANOMMO_CHEAT_MEASURE_H_
