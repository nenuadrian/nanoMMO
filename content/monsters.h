#ifndef NANOMMO_CONTENT_MONSTERS_H_
#define NANOMMO_CONTENT_MONSTERS_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "content/monsters.pb.h"
#include "terrain/lands.h"

namespace nanommo {

/// The kinds of monster, by id, and where they spawn.
struct MonsterTable {
  std::map<uint32_t, monsters::Kind> kinds;
  std::vector<monsters::Spawn> spawns;
  std::vector<monsters::Camps> camps;
};

/// The shortest a monster's skill shows before it lands: long enough to see
/// it coming, and step out.
inline constexpr std::chrono::milliseconds kShortestTelegraph{500};

/// The monsters in text format, or what's wrong with them: a field
/// monsters.proto doesn't have, a kind with no id or with another's, loot
/// with a chance of nothing or more than one, or from more to fewer, gold
/// from more to less, a skill with no id or with another's, with no size, a
/// range past its size, a telegraph shorter than kShortestTelegraph, or a
/// cooldown shorter than its telegraph, a spawn or camps of a kind or in a
/// land that isn't there, or a spawn whose leash reaches the middle of its
/// land.
absl::StatusOr<MonsterTable> ParseMonsters(std::string_view text);

/// Whether the place is in the skill's shape, aimed from where the monster
/// stood, the way it faced, as a yaw: what it lands on, and what a player
/// steps out of.
bool InShape(const monsters::Skill& skill, Place from, float yaw, Place place);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_MONSTERS_H_
