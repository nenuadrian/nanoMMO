#ifndef NANOMMO_CONTENT_NPCS_H_
#define NANOMMO_CONTENT_NPCS_H_

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "content/npcs.pb.h"

namespace nanommo {

/// The kinds of NPC, by id, and where each NPC stands.
struct NpcTable {
  std::map<uint32_t, npcs::Kind> kinds;
  std::vector<npcs::Spawn> spawns;
};

/// The NPCs in text format, or what's wrong with them: a field npcs.proto
/// doesn't have, a kind with no id or with another's, or a spawn of a kind or
/// in a land that isn't there.
absl::StatusOr<NpcTable> ParseNpcs(std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_NPCS_H_
