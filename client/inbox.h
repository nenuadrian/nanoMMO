#ifndef NANOMMO_CLIENT_INBOX_H_
#define NANOMMO_CLIENT_INBOX_H_

#include <cstdint>
#include <deque>
#include <optional>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"

namespace nanommo {

/// What a world server has sent us, filed by kind, for the game to read each
/// frame: the world, put together from its parts, and the chat, our bag, our
/// quests, our guild, our trade, our duel, the moves and the events, which
/// each come on their own. Whatever carries the messages, a socket on a
/// computer or a WebSocket in a browser, hands them to one of these.
class Inbox {
 public:
  /// Files one of the server's messages, or returns why it isn't one.
  absl::Status Take(std::string_view message);

  /// A part of the world that came on its own, as a datagram brings it.
  void Add(const protocol::World& part);

  const protocol::World& world() const { return world_; }
  /// The newest lines said in the world, oldest first.
  const std::deque<protocol::Chat>& chat() const { return chat_; }
  int64_t whole() const { return parts_.whole(); }
  const std::optional<protocol::Move>& move() const { return move_; }
  const protocol::Inventory& inventory() const { return inventory_; }
  const protocol::Quests& quests() const { return quests_; }
  const protocol::Guild& guild() const { return guild_; }
  const protocol::Trade& trade() const { return trade_; }
  const protocol::Duel& duel() const { return duel_; }

  /// The events since the last time, oldest first, handed over once.
  std::vector<protocol::Event> TakeEvents();
  /// The guild that asked us to join since the last time, once.
  std::optional<protocol::GuildInvite> TakeInvite();

 private:
  Parts parts_;
  protocol::World world_;
  std::deque<protocol::Chat> chat_;
  std::optional<protocol::Move> move_;
  protocol::Inventory inventory_;
  protocol::Quests quests_;
  protocol::Guild guild_;
  protocol::Trade trade_;
  protocol::Duel duel_;
  std::optional<protocol::GuildInvite> invite_;
  std::vector<protocol::Event> events_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_INBOX_H_
