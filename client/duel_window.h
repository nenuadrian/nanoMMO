#ifndef NANOMMO_CLIENT_DUEL_WINDOW_H_
#define NANOMMO_CLIENT_DUEL_WINDOW_H_

#include <cstdint>
#include <optional>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The countdown's whole seconds, rounded up, so that it says 1 until the
/// duel starts.
uint32_t CountdownSeconds(uint32_t milliseconds);

/// A duel on the screen: a question when someone asks you, with Duel and Not
/// now, the countdown's seconds in the middle of the screen, and Fight! as it
/// starts. Who won goes in the chat, from the server.
class DuelWindow {
 public:
  /// How long Fight! stays up, in seconds, as it fades.
  static constexpr double kFight = 1.0;

  /// How long an answer hides the question, in seconds, while the server
  /// hears it. If it's still asking after that, as when it turned a yes down,
  /// the question's back.
  static constexpr double kAnswering = 1.0;

  /// Draws what the duel needs now. Returns the player's answer, once.
  std::optional<protocol::DuelAction> Build(const protocol::Duel& duel);

 private:
  protocol::DuelState state_ = protocol::DUEL_NONE;
  /// When the duel started, by ImGui's clock.
  double started_ = -kFight;
  /// When we answered the ask that's up, by ImGui's clock.
  double answered_ = -kAnswering;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_DUEL_WINDOW_H_
