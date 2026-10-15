#ifndef NANOMMO_PROTOCOL_CHAT_H_
#define NANOMMO_PROTOCOL_CHAT_H_

#include <cstddef>

namespace nanommo {

/// The longest line a player can say, in bytes. A character takes one to
/// four bytes in UTF-8, so that's at least fifty characters.
constexpr std::size_t kMaxChatLength = 200;

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_CHAT_H_
