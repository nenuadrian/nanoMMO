#ifndef NANOMMO_CONTENT_SKILLS_H_
#define NANOMMO_CONTENT_SKILLS_H_

#include <cstdint>
#include <map>
#include <set>
#include <string_view>

#include "absl/status/statusor.h"
#include "content/skills.pb.h"

namespace nanommo {

/// The skills in the skills file, by id, the ones each new player knows, and
/// the swing on Space, which is one of them.
struct SkillTable {
  std::map<uint32_t, skills::Skill> by_id;
  std::set<uint32_t> starting;
  uint32_t attack = 0;
};

/// The skills in text format, or what's wrong with them: a field skills.proto
/// doesn't have, a skill with no id or with another's, one with no level, one
/// that lands after it's over, blows or a move whose keys go back in time or
/// run on after it's over, or a starting skill or a swing that isn't there.
absl::StatusOr<SkillTable> ParseSkills(std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_SKILLS_H_
