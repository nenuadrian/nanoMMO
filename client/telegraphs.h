#ifndef NANOMMO_CLIENT_TELEGRAPHS_H_
#define NANOMMO_CLIENT_TELEGRAPHS_H_

#include <cstdint>
#include <map>

#include "content/monsters.h"
#include "protocol/nanommo.pb.h"
#include "terrain/lands.h"

namespace nanommo {

/// A monster's skill on its way: its shape, where the monster stood and the
/// way it faced as it started, and how long until it lands, in seconds.
struct Telegraph {
  monsters::Skill skill;
  Place at;
  float yaw = 0.0f;
  float left = 0.0f;

  /// How far its shape has filled, from 0 as it shows to 1 as it lands.
  float Filled() const;
};

/// The monsters' skills on their way, from the worlds the server sends, with
/// the skills of each kind of monster from the content.
class Telegraphs {
 public:
  /// A world can come this late, in seconds, and say that a skill has more
  /// left than we've counted down to. One that says more is the next skill.
  static constexpr float kLate = 0.25f;

  explicit Telegraphs(const MonsterTable& monsters) : kinds_(monsters.kinds) {}

  /// Follows the newest world, the seconds since the last frame on. A skill
  /// that's new shows where its monster stands, the way it faces, and stays
  /// there however the monster moves. Between worlds, each counts down by
  /// itself. One goes once it's landed, or its monster dies or goes.
  void Update(const protocol::World& world, float seconds);

  /// By the monster's id.
  const std::map<uint32_t, Telegraph>& shown() const { return shown_; }

 private:
  std::map<uint32_t, monsters::Kind> kinds_;
  std::map<uint32_t, Telegraph> shown_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TELEGRAPHS_H_
