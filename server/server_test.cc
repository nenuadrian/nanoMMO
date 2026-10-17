#include "server/server.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/ip/address.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "gtest/gtest.h"
#include "server/accounts.h"
#include "server/test_services.h"

namespace {

// The certificates the protocol tests made, with our own authority.
constexpr char kTls[] = NANOMMO_TEST_TLS "/ours/";

// A player, the way the client is one: over TLS, with a token. It keeps the
// last world the server sent.
class TestPlayer {
 public:
  TestPlayer(asio::io_context& io, asio::ssl::context& tls, uint16_t port,
             std::string token)
      : stream_(io, tls) {
    stream_.set_verify_callback(asio::ssl::host_name_verification("localhost"));
    stream_.lowest_layer().async_connect(
        {asio::ip::make_address("127.0.0.1"), port},
        [this, token = std::move(token)](std::error_code error) {
          if (error) {
            closed_ = true;
            return;
          }
          stream_.async_handshake(
              asio::ssl::stream_base::client,
              [this, token](std::error_code error) { Greet(token, error); });
        });
  }

  // Whether the last world the server sent has the player in it.
  bool Sees(uint32_t id) const {
    return std::ranges::any_of(world_.players(),
                               [id](const nanommo::protocol::Player& player) {
                                 return player.id() == id;
                               });
  }

  const nanommo::protocol::World& world() const { return world_; }

  bool closed() const { return closed_; }

  void Leave() {
    std::error_code ignored;
    stream_.lowest_layer().close(ignored);
  }

 private:
  void Greet(const std::string& token, std::error_code error) {
    if (error) {
      closed_ = true;
      return;
    }
    nanommo::protocol::Hello hello;
    hello.set_token(token);
    hello_ = nanommo::Frame(hello);
    asio::async_write(stream_, asio::buffer(hello_),
                      [](std::error_code, std::size_t) {});
    Read();
  }

  void Read() {
    stream_.async_read_some(
        asio::buffer(buffer_), [this](std::error_code error, std::size_t size) {
          if (error) {
            closed_ = true;
            return;
          }
          reader_.Append({buffer_.data(), size});
          while (std::optional<std::string> message = reader_.Next()) {
            nanommo::protocol::World world;
            // Chat comes in worlds of its own, with nobody in them.
            if (world.ParseFromString(*message) && world.chat().empty()) {
              world_ = std::move(world);
            }
          }
          Read();
        });
  }

  nanommo::TlsStream stream_;
  std::string hello_;
  std::array<char, 4096> buffer_;
  nanommo::FrameReader reader_;
  nanommo::protocol::World world_;
  bool closed_ = false;
};

class ServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    redis_ = nanommo::testing::TestRedis();
    if (!database_ || !redis_) {
      GTEST_SKIP() << "set NANOMMO_MYSQL_HOST and NANOMMO_REDIS_HOST to test "
                      "the server";
    }
    alice_ = NewAccount();
    bob_ = NewAccount();
    tokens_ = std::make_unique<nanommo::Tokens>(*redis_);
    players_ = std::make_unique<nanommo::Players>(*database_, *redis_);
    // Nobody asks it for a story, as the world has no NPCs.
    storyteller_ = std::make_unique<nanommo::Storyteller>(io_, "127.0.0.1");
    server_tls_ = nanommo::ServerTls(absl::StrCat(kTls, "server.crt"),
                                     absl::StrCat(kTls, "server.key"));
    client_tls_ = nanommo::ClientTls(absl::StrCat(kTls, "ca.crt"));
    ASSERT_TRUE(server_tls_.ok() && client_tls_.ok());

    absl::StatusOr<std::unique_ptr<nanommo::Server>> server =
        nanommo::Server::Create(
            io_, 0, *server_tls_, std::chrono::milliseconds(50), world_, 1,
            content_, metrics_, *tokens_, *players_, *storyteller_);
    ASSERT_TRUE(server.ok()) << server.status();
    server_ = *std::move(server);
  }

  void TearDown() override {
    for (const uint32_t id : accounts_) {
      const std::string key = absl::StrCat("player:", world_, ":", id);
      // The server saved the player in Redis, for the saver to take to MySQL.
      EXPECT_TRUE(
          redis_->Pipeline({{"DEL", key}, {"SREM", "players:unsaved", key}})
              .ok());
      EXPECT_TRUE(
          database_
              ->Run("DELETE FROM accounts WHERE id = ?", {absl::StrCat(id)})
              .ok());
    }
  }

  uint32_t NewAccount() {
    nanommo::Accounts accounts(*database_);
    const absl::StatusOr<uint32_t> id =
        accounts.Create(nanommo::testing::TestName(), "hash");
    EXPECT_TRUE(id.ok()) << id.status();
    accounts_.push_back(id.value_or(0));
    return accounts_.back();
  }

  std::string Token(uint32_t account) {
    const absl::StatusOr<std::string> token = tokens_->Issue(account);
    EXPECT_TRUE(token.ok()) << token.status();
    return token.value_or("");
  }

  // Runs the server and the players until done says so, for five seconds at
  // most.
  bool RunUntil(const std::function<bool()>& done) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
      if (io_.run_one_until(deadline) == 0) {
        return false;
      }
    }
    return true;
  }

  // First, so that it's destroyed last, as the server's sockets and timers
  // need it while they close.
  asio::io_context io_;
  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  std::vector<uint32_t> accounts_;
  uint32_t alice_ = 0;
  uint32_t bob_ = 0;
  std::unique_ptr<nanommo::Tokens> tokens_;
  std::unique_ptr<nanommo::Players> players_;
  std::unique_ptr<nanommo::Storyteller> storyteller_;
  const std::string world_ = nanommo::testing::TestName();
  // No items, monsters or NPCs: nothing in the world but the players.
  const nanommo::Content content_{};
  nanommo::Metrics metrics_;
  absl::StatusOr<asio::ssl::context> server_tls_;
  absl::StatusOr<asio::ssl::context> client_tls_;
  std::unique_ptr<nanommo::Server> server_;
};

}  // namespace

TEST_F(ServerTest, PlayersSeeEachOtherUntilOneLeaves) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));
  EXPECT_EQ(alice.world().you(), alice_);
  EXPECT_EQ(server_->players(), 1);

  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  EXPECT_TRUE(RunUntil([&] { return alice.Sees(bob_) && bob.Sees(alice_); }));
  EXPECT_EQ(server_->players(), 2);

  bob.Leave();
  EXPECT_TRUE(
      RunUntil([&] { return server_->players() == 1 && !alice.Sees(bob_); }));
}

TEST_F(ServerTest, AnAccountPlaysOnceAtATime) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  TestPlayer again(io_, *client_tls_, server_->port(), Token(alice_));
  EXPECT_TRUE(RunUntil([&] { return again.closed(); }));
  EXPECT_EQ(server_->players(), 1);

  alice.Leave();
  ASSERT_TRUE(RunUntil([&] { return server_->players() == 0; }));
  TestPlayer later(io_, *client_tls_, server_->port(), Token(alice_));
  EXPECT_TRUE(RunUntil([&] { return later.Sees(alice_); }));
}
