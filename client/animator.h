#ifndef NANOMMO_CLIENT_ANIMATOR_H_
#define NANOMMO_CLIENT_ANIMATOR_H_

#include <raylib.h>

#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

struct Figure {
  uint32_t id = 0;
  std::string name;
  bool you = false;
  bool monster = false;
  bool npc = false;
  /// A monster's or an NPC's kind, by its id in the content folder.
  uint32_t kind = 0;
  /// An NPC's look, until NPCs have models: a ball of this colour and radius,
  /// in metres. A monster is tinted this colour.
  Color color = WHITE;
  float radius = 0.0f;
  /// Whether the player can talk to it, by clicking it.
  bool talks = false;
  /// The skills an NPC teaches, by their ids, when it's a trainer.
  std::vector<uint32_t> teaches;
  int level = 1;
  int health = 0;
  int max_health = 0;
  /// What a player holds, by its id in the items file.
  uint32_t weapon = 0;
  Vector3 position{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  protocol::Motion from = protocol::MOTION_IDLE;
  /// While a motion is a skill's, which skill, by its id in the skills file.
  uint32_t from_skill = 0;
  float from_time = 0.0f;
  protocol::Motion to = protocol::MOTION_IDLE;
  uint32_t to_skill = 0;
  float to_time = 0.0f;
  float blend = 1.0f;
};

class Animator {
 public:
  /// Monsters and NPCs come by kind, so the animator looks up their names and
  /// looks in the content.
  explicit Animator(Content content = Content())
      : content_(std::move(content)) {}

  static constexpr float kBlendTime = 0.25f;

  /// The share of the gap still left after a second is e to the minus this.
  static constexpr float kFollowRate = 15.0f;

  static constexpr float kTurnRate = 4.0f * std::numbers::pi_v<float>;

  std::vector<Figure> Update(const protocol::World& world, float seconds);

 private:
  Content content_;
  // Players, monsters and NPCs each have ids of their own, so they're kept
  // apart.
  std::map<uint32_t, Figure> players_;
  std::map<uint32_t, Figure> monsters_;
  std::map<uint32_t, Figure> npcs_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ANIMATOR_H_
