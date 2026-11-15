#ifndef NANOMMO_CLIENT_LINK_H_
#define NANOMMO_CLIENT_LINK_H_

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Where the client's world comes from, and where what the player does goes:
/// a world server, over the network, in Connection.
class Link {
 public:
  virtual ~Link() = default;

  /// Takes in what's arrived, without waiting.
  virtual void Poll() = 0;

  virtual void Send(const protocol::Input& input) = 0;

  virtual bool connected() const = 0;

  virtual const protocol::World& world() const = 0;

  /// The newest lines said in the world, oldest first.
  virtual const std::deque<protocol::Chat>& chat() const = 0;

  /// The newest world that's come whole, by when it was sent, for the server
  /// to leave out what hasn't changed since.
  virtual int64_t whole() const = 0;

  /// Where the world's instance told us to go, once it hands its players over.
  virtual const std::optional<protocol::Move>& move() const = 0;

  /// What we carry and wear, as the world last sent it.
  virtual const protocol::Inventory& inventory() const = 0;
  virtual const protocol::Quests& quests() const = 0;
  /// Our guild, with no name for none.
  virtual const protocol::Guild& guild() const = 0;
  /// Our trade, with nobody in it for none.
  virtual const protocol::Trade& trade() const = 0;

  /// What happened near us since the last time, oldest first, which it hands
  /// over, as each event comes once.
  virtual std::vector<protocol::Event> TakeEvents() = 0;

  /// The guild that asked us to join since the last time, once.
  virtual std::optional<protocol::GuildInvite> TakeInvite() = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LINK_H_
