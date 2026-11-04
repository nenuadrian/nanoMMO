#ifndef NANOMMO_CLIENT_HOTBAR_H_
#define NANOMMO_CLIENT_HOTBAR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "client/animator.h"
#include "content/skills.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The skills along the bottom of the screen: the swing first, on Space, then
/// the others in the order of their levels, each on a number key: 1 for the
/// first, and so on, up to nine, and then a row above it, on the same keys
/// with Shift.
class Hotbar {
 public:
  /// A row's worth of number keys.
  static constexpr int kRow = 9;
  static constexpr int kMostSlots = 2 * kRow;

  explicit Hotbar(const SkillTable& skills);

  /// Draws each skill with its icon and its key, and how long until it's
  /// ready, or, while the player doesn't know it, the level it needs, or Learn
  /// once they've reached it. A skill that hits one monster is red while
  /// they've picked none, by its id in target, as the server runs them up to
  /// the one they picked, however far. Returns the skill the player used, with
  /// its key or a click.
  std::optional<uint32_t> Build(const std::vector<Figure>& figures,
                                const protocol::World& world,
                                uint32_t target) const;

 private:
  std::vector<skills::Skill> slots_;
  /// Whether the first slot is the swing.
  bool swing_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_HOTBAR_H_
