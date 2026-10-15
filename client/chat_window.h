#ifndef NANOMMO_CLIENT_CHAT_WINDOW_H_
#define NANOMMO_CLIENT_CHAT_WINDOW_H_

#include <array>
#include <deque>
#include <optional>
#include <string>

#include "protocol/chat.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

class ChatWindow {
 public:
  /// Draws the lines said in the world, over a field to type in, which Enter
  /// goes to. Returns what the player says, once Enter sends it.
  std::optional<std::string> Build(const std::deque<protocol::Chat>& lines);

 private:
  // One more for the zero at the end.
  std::array<char, kMaxChatLength + 1> typing_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHAT_WINDOW_H_
