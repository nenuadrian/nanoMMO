#ifndef NANOMMO_CLIENT_ANIMATOR_H_
#define NANOMMO_CLIENT_ANIMATOR_H_

#include <raylib.h>

#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

struct Figure {
  uint32_t id = 0;
  std::string name;
  bool you = false;
  bool monster = false;
  int level = 1;
  int health = 0;
  int max_health = 0;
  Vector3 position{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  protocol::Motion from = protocol::MOTION_IDLE;
  float from_time = 0.0f;
  protocol::Motion to = protocol::MOTION_IDLE;
  float to_time = 0.0f;
  float blend = 1.0f;
};

class Animator {
 public:
  static constexpr float kBlendTime = 0.25f;

  /// The share of the gap still left after a second is e to the minus this.
  static constexpr float kFollowRate = 15.0f;

  static constexpr float kTurnRate = 4.0f * std::numbers::pi_v<float>;

  std::vector<Figure> Update(const protocol::World& world, float seconds);

 private:
  // Players and monsters each have ids of their own, so they're kept apart.
  std::map<uint32_t, Figure> players_;
  std::map<uint32_t, Figure> monsters_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ANIMATOR_H_
