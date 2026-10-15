#ifndef NANOMMO_CLIENT_TALK_WINDOW_H_
#define NANOMMO_CLIENT_TALK_WINDOW_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "protocol/chat.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A window to talk to an NPC, such as the storyteller, which opens when the
/// player clicks it.
class TalkWindow {
 public:
  void Open(uint32_t npc, std::string name);

  /// Draws the window, while it's open. Returns what the player says to the
  /// NPC, once they ask, and closes.
  std::optional<protocol::Talk> Build();

 private:
  std::optional<uint32_t> npc_;
  std::string name_;
  bool focus_ = false;
  // One more for the zero at the end.
  std::array<char, kMaxChatLength + 1> words_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TALK_WINDOW_H_
