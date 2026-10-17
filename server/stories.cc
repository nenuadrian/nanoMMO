#include "server/stories.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "protocol/chat.h"

namespace nanommo {

namespace {

constexpr std::string_view kSentenceEnds = ".!?";

// Whether the word ends a sentence, with one of those, or with one of those
// and the quote that closes what someone says.
bool EndsSentence(std::string_view word) {
  if (word.ends_with('"')) {
    word.remove_suffix(1);
  }
  return !word.empty() &&
         kSentenceEnds.find(word.back()) != std::string_view::npos;
}

// Where to cut a word longer than a line. In UTF-8, each byte after a
// character's first starts with the bits 10, so the cut goes back past those.
std::size_t Cut(std::string_view word) {
  std::size_t cut = kMaxChatLength;
  while (cut > 0 && (static_cast<unsigned char>(word[cut]) & 0xC0) == 0x80) {
    --cut;
  }
  // Nothing but those isn't UTF-8, so any cut will do.
  return cut > 0 ? cut : kMaxChatLength;
}

}  // namespace

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

std::vector<std::string> StoryLines(std::string_view story) {
  std::size_t end = story.find_last_of(kSentenceEnds);
  if (end != std::string_view::npos) {
    // A sentence can end inside the quotes of what someone says.
    if (end + 1 < story.size() && story[end + 1] == '"') {
      ++end;
    }
    story = story.substr(0, end + 1);
  }
  std::vector<std::string> lines;
  std::string line;
  // How much of the line is whole sentences.
  std::size_t sentences = 0;
  for (std::string_view word :
       absl::StrSplit(story, absl::ByAsciiWhitespace(), absl::SkipEmpty())) {
    while (word.size() > kMaxChatLength) {
      if (!line.empty()) {
        lines.push_back(line);
        line.clear();
        sentences = 0;
      }
      const std::size_t cut = Cut(word);
      lines.emplace_back(word.substr(0, cut));
      word.remove_prefix(cut);
    }
    while (!line.empty() && line.size() + 1 + word.size() > kMaxChatLength) {
      const std::size_t cut = sentences > 0 ? sentences : line.size();
      lines.push_back(line.substr(0, cut));
      line.erase(0, std::min(cut + 1, line.size()));
      sentences = 0;
    }
    if (!line.empty()) {
      line += ' ';
    }
    line += word;
    if (EndsSentence(word)) {
      sentences = line.size();
    }
  }
  if (!line.empty()) {
    lines.push_back(line);
  }
  return lines;
}

}  // namespace nanommo
