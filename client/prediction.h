#ifndef NANOMMO_CLIENT_PREDICTION_H_
#define NANOMMO_CLIENT_PREDICTION_H_

#include <cstddef>
#include <cstdint>
#include <deque>

#include "protocol/nanommo.pb.h"
#include "terrain/terrain.h"

namespace nanommo {

/// Where you are before the server says so. Each tick's keys walk you at
/// once, as the server will when they reach it, and each world from the
/// server puts you where it has you, after the last input it applied, and
/// walks you again through the ones it hasn't yet.
class Prediction {
 public:
  /// A tick of the keys sent with the sequence: a direction, which is zero
  /// for standing still, and whether you run.
  void Walk(const Terrain& terrain, uint32_t sequence, float direction_x,
            float direction_z, bool run, float seconds);

  /// Where the server has you, after the input it acked, and how long each of
  /// its ticks is. A small difference from where we had you goes away over a
  /// moment, and a large one, such as going through a gate, at once.
  void Correct(const Terrain& terrain, float x, float z, uint32_t acked,
               float seconds);

  /// Fades what the last corrections moved, over the frame's time.
  void Settle(float seconds);

  /// Where we have you, and where to draw you, with what's left of a
  /// correction still to fade.
  float x() const { return x_; }
  float z() const { return z_; }
  float shown_x() const { return x_ + error_x_; }
  float shown_z() const { return z_ + error_z_; }
  bool placed() const { return placed_; }

  /// Inputs sent and not yet acked.
  std::size_t pending() const { return pending_.size(); }

 private:
  struct Keys {
    uint32_t sequence = 0;
    float x = 0.0f;
    float z = 0.0f;
    bool run = false;
  };
  std::deque<Keys> pending_;
  float x_ = 0.0f;
  float z_ = 0.0f;
  float error_x_ = 0.0f;
  float error_z_ = 0.0f;
  bool placed_ = false;
};

/// The world as the server sent it, with you where the prediction draws you,
/// and walking, running or standing as your keys say, facing the way they
/// go, unless the server has you doing something else, such as a swing.
protocol::World Predicted(const protocol::World& world,
                          const Prediction& prediction, float direction_x,
                          float direction_z, bool run);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PREDICTION_H_
