#ifndef NANOMMO_CLIENT_CONNECTION_H_
#define NANOMMO_CLIENT_CONNECTION_H_

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "client/link.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "protocol/tls.h"

namespace nanommo {

/// A world server, over TLS, with the worlds over UDP or TCP.
class Connection final : public Link {
 public:
  /// Connects to the server over TLS, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Connection>> Create(
      const std::string& host, uint16_t port, asio::ssl::context& tls,
      const std::string& token, bool udp);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  void Poll() override;
  void Send(const protocol::Input& input) override;
  bool connected() const override { return connected_; }
  const protocol::World& world() const override { return world_; }
  const std::deque<protocol::Chat>& chat() const override { return chat_; }
  int64_t whole() const override { return parts_.whole(); }
  const std::optional<protocol::Move>& move() const override { return move_; }
  const protocol::Inventory& inventory() const override { return inventory_; }
  const protocol::Quests& quests() const override { return quests_; }
  const protocol::Guild& guild() const override { return guild_; }
  const protocol::Trade& trade() const override { return trade_; }
  std::vector<protocol::Event> TakeEvents() override;
  std::optional<protocol::GuildInvite> TakeInvite() override;

 private:
  Connection(asio::ssl::context& tls, const std::string& token);

  absl::Status Start(const std::string& host, uint16_t port,
                     const std::string& token, bool udp);
  void Read();
  void HelloOverUdp();
  void ReceiveOverUdp();
  void Receive(const protocol::World& world);
  void See(const protocol::World& events);
  void Hear(const protocol::World& chat);

  asio::io_context io_;
  TlsStream stream_;
  asio::ip::udp::socket udp_socket_;
  asio::steady_timer hello_timer_;
  std::string token_;
  std::array<char, 4096> buffer_;
  std::array<char, 2048> datagram_;
  FrameReader reader_;
  Parts parts_;
  protocol::World world_;
  std::deque<protocol::Chat> chat_;
  std::optional<protocol::Move> move_;
  protocol::Inventory inventory_;
  protocol::Quests quests_;
  protocol::Guild guild_;
  protocol::Trade trade_;
  std::optional<protocol::GuildInvite> invite_;
  std::vector<protocol::Event> events_;
  bool udp_worlds_ = false;
  bool connected_ = true;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CONNECTION_H_
