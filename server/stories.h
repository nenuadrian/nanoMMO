#ifndef NANOMMO_SERVER_STORIES_H_
#define NANOMMO_SERVER_STORIES_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nanommo {

/// How many words mvGPT learned to write a story from.
constexpr std::size_t kStoryWords = 3;

/// mvGPT's prompt for a story with the words in it, the way it learned them:
/// "Words: dragon, castle, gold\nStory:". It takes the first kStoryWords
/// words, split at spaces and commas, in lowercase. Nothing if there are none.
std::optional<std::string> StoryPrompt(std::string_view words);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_STORIES_H_
