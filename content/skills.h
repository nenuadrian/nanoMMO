#ifndef NANOMMO_CONTENT_SKILLS_H_
#define NANOMMO_CONTENT_SKILLS_H_

#include <cstdint>
#include <map>
#include <string_view>

#include "absl/status/statusor.h"
#include "content/skills.pb.h"

namespace nanommo {

/// The skills in the skills file, by id.
struct SkillTable {
  std::map<uint32_t, skills::Skill> by_id;
};

/// The skills in text format, or what's wrong with them: a field skills.proto
/// doesn't have, a skill with no id or with another's, one with no level, or
/// one that lands after it's over.
absl::StatusOr<SkillTable> ParseSkills(std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_SKILLS_H_
