#ifndef NANOMMO_CLIENT_REACTIONS_H_
#define NANOMMO_CLIENT_REACTIONS_H_

#include <cstdint>
#include <map>
#include <vector>

#include "client/animator.h"

namespace nanommo {

/// How the monsters take the blows that land on them: each lights up, and
/// rocks back away from whoever hit it, and one thrown up tumbles as it goes.
class Reactions {
 public:
  /// How long a flash takes to fade, in seconds.
  static constexpr float kFlashTime = 0.15f;
  /// How far a blow rocks a monster back at most, in radians, and how fast it
  /// gets there and comes back.
  static constexpr float kRock = 0.3f;
  static constexpr float kRockIn = 0.03f;
  static constexpr float kRockOut = 0.12f;
  /// How far over a monster in the air tips, in radians, for each metre up.
  static constexpr float kTumble = 0.6f;
  /// How long a blow's kept, in seconds: long enough for a monster it threw
  /// up to come down again, tipped the way it sent it.
  static constexpr float kHeld = 2.0f;

  /// The player hit the monster, both by their ids.
  void Hit(uint32_t monster, uint32_t player);

  /// Moves each blow's reaction on.
  void Update(float seconds);

  /// Lights up, rocks and tips the monsters, as their blows have them now.
  void Apply(std::vector<Figure>& figures);

 private:
  struct Blow {
    uint32_t player = 0;
    /// Seconds since it landed.
    float age = 0.0f;
    /// Away from the player, as a yaw, once we've seen where they are.
    float away = 0.0f;
    bool aimed = false;
  };
  /// The last blow on each monster, by its id.
  std::map<uint32_t, Blow> blows_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_REACTIONS_H_
