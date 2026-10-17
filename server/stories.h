#ifndef NANOMMO_SERVER_STORIES_H_
#define NANOMMO_SERVER_STORIES_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nanommo {

/// How many words mvGPT learned to write a story from.
constexpr std::size_t kStoryWords = 3;

/// mvGPT's prompt for a story with the words in it, the way it learned them:
/// "Words: dragon, castle, gold\nStory:". It takes the first kStoryWords
/// words, split at spaces and commas, in lowercase. Nothing if there are none.
std::optional<std::string> StoryPrompt(std::string_view words);

/// The story as lines of chat, each of at most kMaxChatLength bytes, broken
/// after the last sentence that fits, or between words in a sentence that
/// doesn't. A word longer than a line gets lines of its own, cut between two
/// of its characters. A story that ran out of tokens stops in the middle of a
/// sentence, so the story ends at its last full stop, or question or
/// exclamation mark, when it has one.
std::vector<std::string> StoryLines(std::string_view story);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_STORIES_H_
