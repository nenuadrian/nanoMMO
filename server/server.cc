#include "server/server.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "server/listen.h"
#include "tracy/Tracy.hpp"

namespace nanommo {

namespace {

protocol::Motion ToProtocol(Motion motion) {
  switch (motion) {
    case Motion::kWalk:
      return protocol::MOTION_WALK;
    case Motion::kRun:
      return protocol::MOTION_RUN;
    case Motion::kIdle:
      break;
  }
  return protocol::MOTION_IDLE;
}

}  // namespace

absl::StatusOr<std::unique_ptr<Server>> Server::Create(
    asio::io_context& io, uint16_t port, std::chrono::nanoseconds interval,
    Metrics& metrics, Tokens& tokens, Players& players) {
  std::unique_ptr<Server> server =
      absl::WrapUnique(new Server(io, interval, metrics, tokens, players));
  const absl::Status started = server->Start(port);
  if (!started.ok()) {
    return started;
  }
  return server;
}

Server::Server(asio::io_context& io, std::chrono::nanoseconds interval,
               Metrics& metrics, Tokens& tokens, Players& players)
    : acceptor_(io),
      timer_(io),
      interval_(interval),
      metrics_(metrics),
      tokens_(tokens),
      players_(players) {}

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

void Server::SaveAll() {
  for (const auto& [id, player] : world_.players()) {
    const absl::Status saved = players_.Save(id, player);
    if (!saved.ok()) {
      std::cerr << "couldn't save " << player.name << ": " << saved << "\n";
    }
  }
}

void Server::Accept() {
  acceptor_.async_accept([this](std::error_code error,
                                asio::ip::tcp::socket socket) {
    if (!error) {
      auto id = std::make_shared<uint32_t>(0);

      const auto on_message = [this, id](Session& session,
                                         const std::string& bytes) {
        metrics_.AddMessageReceived();
        if (*id == 0) {
          *id = HandleHello(session, bytes);
          return;
        }
        protocol::Input input;
        if (input.ParseFromString(bytes)) {
          world_.SetInput(*id, input.x(), input.z(), input.run());
        }
      };
      const auto on_close = [this, id] {
        if (*id != 0) {
          Leave(*id);
        }
      };
      auto session =
          std::make_shared<Session>(std::move(socket), on_message, on_close);
      session->Start();
    }
    Accept();
  });
}

uint32_t Server::HandleHello(Session& session, const std::string& bytes) {
  protocol::Hello hello;
  const absl::StatusOr<uint32_t> id =
      hello.ParseFromString(bytes) ? tokens_.Redeem(hello.token())
                                   : absl::InvalidArgumentError("not a hello");
  if (!id.ok() || world_.players().contains(*id)) {
    std::cout << "refused a client: unknown token, or already playing"
              << std::endl;
    session.Close();
    return 0;
  }
  const absl::StatusOr<Player> player = players_.Load(*id);
  if (!player.ok()) {
    std::cerr << "couldn't load player " << *id << ": " << player.status()
              << "\n";
    session.Close();
    return 0;
  }
  world_.Join(*id, *player);
  sessions_[*id] = session.shared_from_this();
  metrics_.SetPlayers(world_.players().size());
  std::cout << world_.players().at(*id).name << " joined, as player " << *id
            << std::endl;
  return *id;
}

void Server::Leave(uint32_t id) {
  const Player& player = world_.players().at(id);
  std::cout << player.name << " left" << std::endl;
  const absl::Status saved = players_.Save(id, player);
  if (!saved.ok()) {
    std::cerr << "couldn't save " << player.name << ": " << saved << "\n";
  }
  world_.Leave(id);
  sessions_.erase(id);
  metrics_.SetPlayers(world_.players().size());
}

void Server::Tick() {
  ZoneScoped;
  const auto start = std::chrono::steady_clock::now();
  world_.Step(interval_);
  Broadcast();
  metrics_.ObserveTick(std::chrono::steady_clock::now() - start);
  TracyPlot("players", static_cast<int64_t>(world_.players().size()));
  FrameMark;
  // From the last expiry, not from now, so that the ticks don't drift.
  timer_.expires_at(timer_.expiry() + interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
}

void Server::Broadcast() {
  ZoneScoped;
  protocol::World message;
  for (const auto& [id, player] : world_.players()) {
    protocol::Player* sent = message.add_players();
    sent->set_id(id);
    sent->set_x(player.x);
    sent->set_z(player.z);
    sent->set_name(player.name);
    sent->set_yaw(player.yaw);
    sent->set_motion(ToProtocol(player.motion));
  }
  for (const auto& [id, session] : sessions_) {
    message.set_you(id);
    std::string bytes;
    {
      ZoneScopedN("serialize");
      bytes = Frame(message);
    }
    metrics_.AddBytesSent(bytes.size());
    session->Send(std::move(bytes));
  }
}

}  // namespace nanommo
