#ifndef NANOMMO_CONTENT_QUESTS_H_
#define NANOMMO_CONTENT_QUESTS_H_

#include <cstdint>
#include <map>
#include <string_view>

#include "absl/status/statusor.h"
#include "content/quests.pb.h"

namespace nanommo {

/// The quests in text format, by id, or what's wrong with them: a field
/// quests.proto doesn't have, a quest with no id or with another's, one with
/// no giver, one that asks for no kills, or a prize of none of an item.
absl::StatusOr<std::map<uint32_t, quests::Quest>> ParseQuests(
    std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_QUESTS_H_
