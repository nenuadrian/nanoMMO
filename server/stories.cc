#include "server/stories.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace nanommo {

std::optional<std::string> StoryPrompt(std::string_view words) {
  std::vector<std::string> kept;
  for (const std::string_view word :
       absl::StrSplit(words, absl::ByAnyChar(" ,"), absl::SkipEmpty())) {
    if (kept.size() == kStoryWords) {
      break;
    }
    // The stories it learned from give their words in lowercase.
    kept.push_back(absl::AsciiStrToLower(word));
  }
  if (kept.empty()) {
    return std::nullopt;
  }
  return absl::StrCat("Words: ", absl::StrJoin(kept, ", "), "\nStory:");
}

}  // namespace nanommo
