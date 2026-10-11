#include "server/server.h"

#include <array>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"
#include "protocol/clock.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "server/levels.h"
#include "server/listen.h"
#include "server/monsters.h"
#include "tracy/Tracy.hpp"

namespace nanommo {

namespace {

constexpr std::array<std::pair<float, float>, 3> kSkeletons = {{
    {-3.5f, -3.0f},
    {3.5f, -2.5f},
    {3.0f, 3.5f},
}};

protocol::Motion ToProtocol(Motion motion) {
  switch (motion) {
    case Motion::kWalk:
      return protocol::MOTION_WALK;
    case Motion::kRun:
      return protocol::MOTION_RUN;
    case Motion::kAttack:
      return protocol::MOTION_ATTACK;
    case Motion::kDead:
      return protocol::MOTION_DEAD;
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
      udp_(io),
      timer_(io),
      interval_(interval),
      world_(std::random_device()()),
      metrics_(metrics),
      tokens_(tokens),
      players_(players) {}

absl::Status Server::Start(uint16_t port) {
  const absl::Status listening = Listen(acceptor_, port);
  if (!listening.ok()) {
    return listening;
  }
  const absl::Status bound = Bind(udp_, port);
  if (!bound.ok()) {
    return bound;
  }
  // One socket sends all the datagrams, so it gets more room for those on
  // their way out than the two hundred kilobytes Linux gives by default.
  std::error_code ignored;
  udp_.set_option(asio::socket_base::send_buffer_size(4 * 1024 * 1024),
                  ignored);
  for (const auto& [x, z] : kSkeletons) {
    world_.Spawn(Skeleton(x, z));
  }
  Accept();
  Receive();
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
          world_.SetInput(*id, input.x(), input.z(), input.run(),
                          input.attack());
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
  udp_tokens_[hello.token()] = *id;
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
  std::erase_if(udp_tokens_,
                [id](const auto& token) { return token.second == id; });
  udp_clients_.erase(id);
  metrics_.SetPlayers(world_.players().size());
}

void Server::Receive() {
  udp_.async_receive_from(
      asio::buffer(datagram_), sender_,
      [this](std::error_code error, std::size_t size) {
        protocol::Hello hello;
        if (!error &&
            hello.ParseFromArray(datagram_.data(), static_cast<int>(size))) {
          const auto token = udp_tokens_.find(hello.token());
          if (token != udp_tokens_.end()) {
            udp_clients_[token->second] = sender_;
          }
        }
        Receive();
      });
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
    const int level = LevelFor(player.experience).number;
    sent->set_level(level);
    sent->set_health(player.health);
    sent->set_max_health(MaxHealth(level));
  }
  for (const auto& [id, monster] : world_.monsters()) {
    protocol::Monster* sent = message.add_monsters();
    sent->set_id(id);
    sent->set_name(monster.name);
    sent->set_x(monster.x);
    sent->set_z(monster.z);
    sent->set_yaw(monster.yaw);
    sent->set_motion(ToProtocol(monster.motion));
    sent->set_level(monster.level);
    sent->set_health(monster.health);
    sent->set_max_health(monster.max_health);
  }
  message.set_sent(NowMicros());
  // The players and the monsters are the same for everyone, so we serialize
  // them once, and all the sessions share the bytes until each has written
  // them.
  std::shared_ptr<const std::string> players;
  {
    ZoneScopedN("serialize");
    players = std::make_shared<const std::string>(message.SerializeAsString());
  }
  std::vector<std::shared_ptr<const std::string>> parts;
  if (!udp_clients_.empty()) {
    for (const protocol::World& part : Split(message)) {
      parts.push_back(
          std::make_shared<const std::string>(part.SerializeAsString()));
    }
  }
  for (const auto& [id, session] : sessions_) {
    const Level level = LevelFor(world_.players().at(id).experience);
    protocol::World you;
    you.set_you(id);
    you.set_experience(level.experience);
    you.set_next_level(level.next);
    std::string own = you.SerializeAsString();
    const auto udp = udp_clients_.find(id);
    if (udp != udp_clients_.end()) {
      // A datagram arrives whole or not at all, so it needs no size in front.
      for (const auto& part : parts) {
        metrics_.AddBytesSent(part->size() + own.size());
        SendTo(udp->second, part, own);
      }
    } else {
      metrics_.AddBytesSent(kHeaderSize + players->size() + own.size());
      session->Send(players, std::move(own));
    }
  }
}

void Server::SendTo(const asio::ip::udp::endpoint& client,
                    std::shared_ptr<const std::string> part, std::string own) {
  auto tail = std::make_shared<const std::string>(std::move(own));
  const std::array buffers{asio::buffer(*part), asio::buffer(*tail)};
  // The handler keeps the bytes alive until the datagram has gone.
  udp_.async_send_to(buffers, client,
                     [part, tail](std::error_code, std::size_t) {});
}

}  // namespace nanommo
