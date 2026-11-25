#include "server/stories.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/log/absl_check.h"
#include "protocol/chat.h"
#include "utf8_range/utf8_validity.h"

// The model can write anything, and each line has to fit in the chat.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  const std::string_view story(reinterpret_cast<const char*>(data), size);
  // Protobuf refuses text that isn't UTF-8, so a line mustn't end inside a
  // character.
  const bool utf8 = utf8_range::IsStructurallyValid(story);
  for (const std::string& line : nanommo::StoryLines(story)) {
    ABSL_CHECK_LE(line.size(), nanommo::kMaxChatLength);
    ABSL_CHECK(!utf8 || utf8_range::IsStructurallyValid(line));
  }
  return 0;
}
