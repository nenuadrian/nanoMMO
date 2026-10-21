#ifndef NANOMMO_CLIENT_HOTBAR_H_
#define NANOMMO_CLIENT_HOTBAR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "client/animator.h"
#include "content/skills.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The skills along the bottom of the screen, in the order of their levels,
/// each on a number key: 1 for the first, and so on, up to nine.
class Hotbar {
 public:
  static constexpr int kMostSlots = 9;

  explicit Hotbar(const SkillTable& skills);

  /// Draws each skill with its icon and its key, and how long until it's
  /// ready, or the level it needs. Returns the skill the player used, with
  /// its key or a click.
  std::optional<uint32_t> Build(const std::vector<Figure>& figures,
                                const protocol::World& world) const;

 private:
  std::vector<skills::Skill> slots_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_HOTBAR_H_
