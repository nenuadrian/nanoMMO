#ifndef NANOMMO_PROTOCOL_CHAT_H_
#define NANOMMO_PROTOCOL_CHAT_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nanommo {

/// The longest line a player can say, in bytes. A character takes one to
/// four bytes in UTF-8, so that's at least fifty characters.
constexpr std::size_t kMaxChatLength = 200;

/// The line as the other players get it: control characters, such as a new
/// line, become spaces, and the spaces at either end go. Nothing if that
/// leaves nothing, or if the line is longer than kMaxChatLength.
std::optional<std::string> CleanChat(std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_CHAT_H_
