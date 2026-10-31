#ifndef NANOMMO_CLIENT_DEATH_SCREEN_H_
#define NANOMMO_CLIENT_DEATH_SCREEN_H_

#include <cstdint>

namespace nanommo {

/// What the player sees while they're dead: the world darkens, and a window
/// in the middle counts down to when they can come back, then lets them,
/// with a click. As they come back, in the middle of the land, the screen
/// goes dark and the world fades back in, so that they don't jump there.
class DeathScreen {
 public:
  /// How dark the world gets while they're dead, from 0 to 1.
  static constexpr float kDim = 0.6f;
  /// In seconds: how long the world takes to darken, and to come back.
  static constexpr float kDarken = 1.0f;
  static constexpr float kReturn = 0.8f;

  /// Moves on by the frame's time, from whether the player's dead.
  void Update(float seconds, bool dead);

  /// How dark the world is, from 0 to 1.
  float dark() const { return dark_; }

  /// Darkens the world, and while the player's dead, draws the window, with
  /// how long they have to wait, in milliseconds. Returns true once they ask
  /// to come back.
  bool Build(bool dead, uint32_t wait_ms) const;

 private:
  float dark_ = 0.0f;
  bool was_dead_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_DEATH_SCREEN_H_
