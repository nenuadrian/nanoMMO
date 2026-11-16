#ifndef NANOMMO_CLIENT_CHAT_WINDOW_H_
#define NANOMMO_CLIENT_CHAT_WINDOW_H_

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

#include "protocol/chat.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What a line typed in the chat says to the guild, when it starts with /g
/// and a space, even if that's nothing, which the server drops, or nothing at
/// all, for a line to the world.
std::optional<std::string> GuildLine(std::string_view typed);

/// Who a line typed in the chat asks to trade with, by their name, when it
/// starts with /trade and a space, or nothing, for any other line.
std::optional<std::string> TradeName(std::string_view typed);

/// What a line typed in the chat does about a duel, when it starts with /duel
/// and a space: yes or no to the one who asked, yield, or, with a name, asks
/// that player. Nothing, for any other line.
std::optional<protocol::DuelAction> DuelLine(std::string_view typed);

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
