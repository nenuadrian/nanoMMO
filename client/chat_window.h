#ifndef NANOMMO_CLIENT_CHAT_WINDOW_H_
#define NANOMMO_CLIENT_CHAT_WINDOW_H_

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>

#include "protocol/chat.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What's said in the world, in the bottom left, over a field to type in,
/// which Enter goes to. Its close button folds it into a button, which says
/// how many lines have come since, and which, or Enter, opens it again.
class ChatWindow {
 public:
  /// Draws the lines, or the button while it's closed. Returns what the
  /// player says, once Enter sends it.
  std::optional<std::string> Build(const std::deque<protocol::Chat>& lines);

  /// How many of the lines came after the last one seen while it was open.
  static std::size_t Unread(const std::deque<protocol::Chat>& lines,
                            const std::optional<protocol::Chat>& seen);

 private:
  bool open_ = true;
  // Opened with Enter, so the field takes the keys at once.
  bool focus_ = false;
  // The newest line while it was last open.
  std::optional<protocol::Chat> seen_;
  // One more for the zero at the end.
  std::array<char, kMaxChatLength + 1> typing_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHAT_WINDOW_H_
