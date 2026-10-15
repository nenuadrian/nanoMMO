#include "protocol/chat.h"

#include <optional>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"

namespace nanommo {

std::optional<std::string> CleanChat(std::string_view text) {
  if (text.size() > kMaxChatLength) {
    return std::nullopt;
  }
  std::string clean(text);
  // UTF-8 writes the characters past ASCII in bytes over 127, which this
  // leaves alone.
  for (char& c : clean) {
    if (absl::ascii_iscntrl(c)) {
      c = ' ';
    }
  }
  absl::StripAsciiWhitespace(&clean);
  if (clean.empty()) {
    return std::nullopt;
  }
  return clean;
}

}  // namespace nanommo
