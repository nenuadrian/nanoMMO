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
/// are, with a name to ask someone in by, for an officer or the leader.
class GuildWindow {
 public:
  bool* mutable_visible() { return &visible_; }
  bool visible() const { return visible_; }
  void Toggle() { visible_ = !visible_; }

  /// A guild asked you to join, which the window asks you about, open or not,
  /// until you answer or the invitation runs out.
  void Invited(protocol::GuildInvite invite);

  /// Draws the question, if a guild asked, and the window, while it's open.
  /// Returns what the player asks for, which the server does, or says in the
  /// chat why not.
  std::optional<protocol::GuildAction> Build(const protocol::Guild& yours);

 private:
  std::optional<protocol::GuildAction> BuildQuestion();

  bool visible_ = false;
  /// The name of a guild to found: up to 24 letters, and the end.
  std::array<char, 25> name_{};
  /// The name of a player to ask in: up to 16 characters, and the end.
  std::array<char, 17> player_{};
  std::optional<protocol::GuildInvite> invite_;
  /// When it came, in ImGui's seconds.
  double invited_at_ = 0.0;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GUILD_WINDOW_H_
