#include "server/server.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "server/listen.h"

namespace nanommo {

absl::StatusOr<std::unique_ptr<Server>> Server::Create(
    asio::io_context& io, uint16_t port, std::chrono::nanoseconds interval,
    Metrics& metrics) {
  std::unique_ptr<Server> server =
      absl::WrapUnique(new Server(io, interval, metrics));
  const absl::Status started = server->Start(port);
  if (!started.ok()) {
    return started;
  }
  return server;
}

Server::Server(asio::io_context& io, std::chrono::nanoseconds interval,
               Metrics& metrics)
    : acceptor_(io), timer_(io), interval_(interval), metrics_(metrics) {}

absl::Status Server::Start(uint16_t port) {
  const absl::Status listening = Listen(acceptor_, port);
  if (!listening.ok()) {
    return listening;
  }
  Accept();
  timer_.expires_after(interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
  return absl::OkStatus();
}

void Server::Accept() {
  acceptor_.async_accept([this](std::error_code error,
                                asio::ip::tcp::socket socket) {
    if (!error) {
      const uint32_t id = world_.Join();
      metrics_.SetPlayers(world_.players().size());
      std::error_code ignored;
      std::cout << "player " << id << " joined from "
                << socket.remote_endpoint(ignored) << std::endl;

      const auto on_message = [this, id](Session&, const std::string& bytes) {
        metrics_.AddMessageReceived();
        protocol::Input input;
        if (input.ParseFromString(bytes)) {
          world_.SetInput(id, input.x(), input.z());
        }
      };
      const auto on_close = [this, id] {
        world_.Leave(id);
        sessions_.erase(id);
        metrics_.SetPlayers(world_.players().size());
        std::cout << "player " << id << " left" << std::endl;
      };
      auto session =
          std::make_shared<Session>(std::move(socket), on_message, on_close);
      sessions_[id] = session;
      session->Start();
    }
    Accept();
  });
}

void Server::Tick() {
  const auto start = std::chrono::steady_clock::now();
  world_.Step(interval_);
  Broadcast();
  metrics_.ObserveTick(std::chrono::steady_clock::now() - start);
  // From the last expiry, not from now, so that the ticks don't drift.
  timer_.expires_at(timer_.expiry() + interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
}

void Server::Broadcast() {
  protocol::World message;
  for (const auto& [id, player] : world_.players()) {
    protocol::Player* sent = message.add_players();
    sent->set_id(id);
    sent->set_x(player.x);
    sent->set_z(player.z);
  }
  for (const auto& [id, session] : sessions_) {
    message.set_you(id);
    std::string bytes = Frame(message);
    metrics_.AddBytesSent(bytes.size());
    session->Send(std::move(bytes));
  }
}

}  // namespace nanommo
