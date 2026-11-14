#include "client/prediction.h"

#include <cmath>
#include <cstddef>

#include "server/walking.h"

namespace nanommo {

namespace {

// A correction further than this, in metres, is a jump, such as through a
// gate, and isn't faded.
constexpr float kJump = 2.0f;

// How fast a correction fades: most of it in a tenth of a second.
constexpr float kFade = 20.0f;

// Five seconds of ticks: a server that hasn't acked any of them for longer,
// say while a world hands its players over, won't, and they'd pile up.
constexpr std::size_t kMostPending = 100;

}  // namespace

void Prediction::Walk(const Terrain& terrain, uint32_t sequence,
                      float direction_x, float direction_z, bool run,
                      float seconds) {
  pending_.push_back({sequence, direction_x, direction_z, run});
  if (pending_.size() > kMostPending) {
    pending_.pop_front();
  }
  if (placed_) {
    WalkAlong(terrain, x_, z_, direction_x, direction_z, run, seconds);
  }
}

void Prediction::Correct(const Terrain& terrain, float x, float z,
                         uint32_t acked, float seconds) {
  while (!pending_.empty() && pending_.front().sequence <= acked) {
    pending_.pop_front();
  }
  const float was_x = x_;
  const float was_z = z_;
  x_ = x;
  z_ = z;
  for (const Keys& keys : pending_) {
    WalkAlong(terrain, x_, z_, keys.x, keys.z, keys.run, seconds);
  }
  // What we drew a moment ago stays where it was, and fades into the new.
  if (placed_ && std::hypot(was_x - x_, was_z - z_) < kJump) {
    error_x_ += was_x - x_;
    error_z_ += was_z - z_;
  } else {
    error_x_ = 0.0f;
    error_z_ = 0.0f;
  }
  placed_ = true;
}

protocol::World Predicted(const protocol::World& world,
                          const Prediction& prediction, float direction_x,
                          float direction_z, bool run) {
  protocol::World shown = world;
  if (!prediction.placed()) {
    return shown;
  }
  for (protocol::Player& player : *shown.mutable_players()) {
    if (player.id() != shown.you()) {
      continue;
    }
    player.set_x(prediction.shown_x());
    player.set_z(prediction.shown_z());
    const protocol::Motion motion = player.motion();
    if (motion == protocol::MOTION_IDLE || motion == protocol::MOTION_WALK ||
        motion == protocol::MOTION_RUN) {
      const bool moving = direction_x != 0.0f || direction_z != 0.0f;
      player.set_motion(!moving ? protocol::MOTION_IDLE
                        : run   ? protocol::MOTION_RUN
                                : protocol::MOTION_WALK);
      if (moving) {
        player.set_yaw(std::atan2(direction_x, direction_z));
      }
    }
  }
  return shown;
}

void Prediction::Settle(float seconds) {
  const float left = std::exp(-kFade * seconds);
  error_x_ *= left;
  error_z_ *= left;
}

}  // namespace nanommo
