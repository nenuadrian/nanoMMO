#ifndef NANOMMO_CLIENT_TRAINER_WINDOW_H_
#define NANOMMO_CLIENT_TRAINER_WINDOW_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "content/skills.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A window with the skills a trainer teaches, which opens when the player
/// clicks the trainer.
class TrainerWindow {
 public:
  explicit TrainerWindow(SkillTable skills) : skills_(std::move(skills)) {}

  void Open(uint32_t npc, std::string name, std::vector<uint32_t> teaches);

  /// Draws the window, while it's open: each skill, with a button to learn
  /// it, once the player has reached its level. Returns the skill the player
  /// asks for.
  std::optional<protocol::Learn> Build(const std::vector<Figure>& figures,
                                       const protocol::World& world);

 private:
  SkillTable skills_;
  std::optional<uint32_t> npc_;
  std::string name_;
  std::vector<uint32_t> teaches_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TRAINER_WINDOW_H_
