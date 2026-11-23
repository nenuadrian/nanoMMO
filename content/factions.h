#ifndef NANOMMO_CONTENT_FACTIONS_H_
#define NANOMMO_CONTENT_FACTIONS_H_

#include <cstdint>
#include <map>
#include <string_view>

#include "absl/status/statusor.h"
#include "content/factions.pb.h"

namespace nanommo {

/// The factions in text format, by id, or what's wrong with them: a field
/// factions.proto doesn't have, a faction with no id or with another's, one
/// with no colour or no herald, or two with one herald.
absl::StatusOr<std::map<uint32_t, factions::Faction>> ParseFactions(
    std::string_view text);

/// The faction a kind of NPC is the herald of, by the kind's id, or nullptr
/// for one that's no herald.
const factions::Faction* HeraldOf(
    const std::map<uint32_t, factions::Faction>& factions, uint32_t kind);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_FACTIONS_H_
