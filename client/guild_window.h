#ifndef NANOMMO_CLIENT_GUILD_WINDOW_H_
#define NANOMMO_CLIENT_GUILD_WINDOW_H_

#include <array>
#include <optional>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// What a rank is called: Leader, Officer or Member.
const char* RankName(protocol::Rank rank);

/// Your guild, in a window of its own, which G opens and closes: a name to
/// found one with while you're in none, and its name and your rank once you
/// are.
class GuildWindow {
 public:
  bool* mutable_visible() { return &visible_; }
  bool visible() const { return visible_; }
  void Toggle() { visible_ = !visible_; }

  /// Draws the window, while it's open. Returns what the player asks for,
  /// which the server does, or says in the chat why not.
  std::optional<protocol::GuildAction> Build(const protocol::Guild& yours);

 private:
  bool visible_ = false;
  /// The name being typed: up to 24 letters, and the end.
  std::array<char, 25> name_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GUILD_WINDOW_H_
