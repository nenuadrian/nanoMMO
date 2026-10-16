#ifndef NANOMMO_CONTENT_MONSTERS_H_
#define NANOMMO_CONTENT_MONSTERS_H_

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "content/monsters.pb.h"

namespace nanommo {

/// The kinds of monster, by id, and where they spawn.
struct MonsterTable {
  std::map<uint32_t, monsters::Kind> kinds;
  std::vector<monsters::Spawn> spawns;
  std::vector<monsters::Camps> camps;
};

/// The monsters in text format, or what's wrong with them: a field
/// monsters.proto doesn't have, a kind with no id or with another's, a spawn
/// or camps of a kind or in a land that isn't there, or a spawn whose leash
/// reaches the middle of its land.
absl::StatusOr<MonsterTable> ParseMonsters(std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_MONSTERS_H_
