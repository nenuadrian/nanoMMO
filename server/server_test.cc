#include "server/server.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/ip/address.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "gtest/gtest.h"
#include "server/accounts.h"
#include "server/characters.h"
#include "server/guilds.h"
#include "server/inventory.h"
#include "server/test_content.h"
#include "server/test_services.h"
#include "server/walking.h"
#include "terrain/lands.h"

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

  /// What the player carries and wears, as the server last sent it.
  const nanommo::protocol::Inventory& bag() const { return bag_; }

  /// The quests the player's on, as the server last sent them, once it has.
  const std::optional<nanommo::protocol::Quests>& quests() const {
    return quests_;
  }

  /// The player's guild, as the server last sent it, once it has.
  const std::optional<nanommo::protocol::Guild>& guild() const {
    return guild_;
  }

  /// The last guild that asked the player to join, once one has.
  const std::optional<nanommo::protocol::GuildInvite>& invite() const {
    return invite_;
  }

  /// The player's trade, as the server last sent it, once it has.
  const std::optional<nanommo::protocol::Trade>& trade() const {
    return trade_;
  }

  /// What the server told the player, which nobody said.
  const std::vector<std::string>& notices() const { return notices_; }

  /// What players said that this player heard, oldest first.
  const std::vector<nanommo::protocol::Chat>& heard() const { return heard_; }

  /// What the player saw happen, oldest first.
  const std::vector<nanommo::protocol::Event>& events() const {
    return events_;
  }

  void Send(const nanommo::protocol::Input& input) {
    auto bytes = std::make_shared<const std::string>(nanommo::Frame(input));
    asio::async_write(stream_, asio::buffer(*bytes),
                      [bytes](std::error_code, std::size_t) {});
  }

  // In one write, so that they arrive together.
  void SendTogether(const std::vector<nanommo::protocol::Input>& inputs) {
    auto bytes = std::make_shared<std::string>();
    for (const nanommo::protocol::Input& input : inputs) {
      *bytes += nanommo::Frame(input);
    }
    asio::async_write(stream_, asio::buffer(*bytes),
                      [bytes](std::error_code, std::size_t) {});
  }

  bool closed() const { return closed_; }

  void Leave() {
    std::error_code ignored;
    stream_.lowest_layer().close(ignored);
  }

  // Sends the input, and leaves as soon as it's sent, as a client that quits
  // straight after it would, before the server's next tick.
  void SendAndLeave(const nanommo::protocol::Input& input) {
    auto bytes = std::make_shared<const std::string>(nanommo::Frame(input));
    asio::async_write(stream_, asio::buffer(*bytes),
                      [this, bytes](std::error_code, std::size_t) { Leave(); });
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
            // Chat, the bag and the quests come in worlds of their own, with
            // nobody in them.
            if (!world.ParseFromString(*message)) {
              continue;
            }
            if (world.has_inventory()) {
              bag_ = world.inventory();
            } else if (world.has_guild()) {
              guild_ = world.guild();
            } else if (world.has_invite()) {
              invite_ = world.invite();
            } else if (world.has_quests()) {
              quests_ = world.quests();
            } else if (world.has_trade()) {
              trade_ = world.trade();
            } else if (!world.events().empty()) {
              events_.insert(events_.end(), world.events().begin(),
                             world.events().end());
            } else if (!world.chat().empty()) {
              for (const nanommo::protocol::Chat& line : world.chat()) {
                if (line.name().empty()) {
                  notices_.push_back(line.text());
                } else {
                  heard_.push_back(line);
                }
              }
            } else {
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
  nanommo::protocol::Inventory bag_;
  std::optional<nanommo::protocol::Quests> quests_;
  std::optional<nanommo::protocol::Guild> guild_;
  std::optional<nanommo::protocol::GuildInvite> invite_;
  std::optional<nanommo::protocol::Trade> trade_;
  std::vector<std::string> notices_;
  std::vector<nanommo::protocol::Chat> heard_;
  std::vector<nanommo::protocol::Event> events_;
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
    guilds_ = std::make_unique<nanommo::Guilds>(*database_, *redis_);
    events_ = std::make_unique<nanommo::Events>(*redis_);
    // Nobody asks it for a story, as the world has no NPCs.
    storyteller_ = std::make_unique<nanommo::Storyteller>(io_, "127.0.0.1");
    server_tls_ = nanommo::ServerTls(absl::StrCat(kTls, "server.crt"),
                                     absl::StrCat(kTls, "server.key"));
    client_tls_ = nanommo::ClientTls(absl::StrCat(kTls, "ca.crt"));
    ASSERT_TRUE(server_tls_.ok() && client_tls_.ok());

    absl::StatusOr<std::unique_ptr<nanommo::Server>> server =
        nanommo::Server::Create(io_, 0, *server_tls_,
                                std::chrono::milliseconds(50), world_, 1,
                                content_, metrics_, *tokens_, *players_,
                                *guilds_, *events_, *storyteller_);
    ASSERT_TRUE(server.ok()) << server.status();
    server_ = *std::move(server);
  }

  void TearDown() override {
    for (const uint32_t id : characters_) {
      const std::string key = absl::StrCat("player:", world_, ":", id);
      // The server saved the player in Redis, for the saver to take to MySQL,
      // and leased them there.
      EXPECT_TRUE(redis_
                      ->Pipeline({{"DEL", key},
                                  {"SREM", "players:unsaved", key},
                                  {"SREM", "items:unsaved", key},
                                  {"DEL", absl::StrCat("guild_invite:", id)},
                                  {"DEL", LeaseKey(id)}})
                      .ok());
    }
    if (redis_) {
      EXPECT_TRUE(redis_->Command({"DEL", "fencing:" + world_}).ok());
      // And recorded their trades, for the saver.
      for (const nanommo::Redis::Entry& trade : Trades()) {
        EXPECT_TRUE(redis_->Command({"XDEL", "trades", trade.id}).ok());
      }
    }
    // Their characters go with them, and their memberships with those, but
    // not the guilds.
    if (database_) {
      EXPECT_TRUE(
          database_->Run("DELETE FROM guilds WHERE world = ?", {world_}).ok());
    }
    for (const uint32_t id : accounts_) {
      EXPECT_TRUE(
          database_
              ->Run("DELETE FROM accounts WHERE id = ?", {absl::StrCat(id)})
              .ok());
    }
  }

  // A new account, with a character in this world, named after it, whose id
  // it returns.
  uint32_t NewAccount() {
    nanommo::Accounts accounts(*database_);
    const std::string name = nanommo::testing::TestName();
    const absl::StatusOr<uint32_t> account = accounts.Create(name, "hash");
    EXPECT_TRUE(account.ok()) << account.status();
    accounts_.push_back(account.value_or(0));
    const absl::StatusOr<uint32_t> id =
        nanommo::Characters(*database_).Create(accounts_.back(), world_, name);
    EXPECT_TRUE(id.ok()) << id.status();
    characters_.push_back(id.value_or(0));
    names_[characters_.back()] = name;
    return characters_.back();
  }

  std::string Token(uint32_t account) {
    const absl::StatusOr<std::string> token = tokens_->Issue(account);
    EXPECT_TRUE(token.ok()) << token.status();
    return token.value_or("");
  }

  std::string LeaseKey(uint32_t id) const {
    return absl::StrCat("playing:", world_, ":", id);
  }

  // The instance whose lease the player is on, if any is.
  std::string LeasedTo(uint32_t id) {
    const std::string lease =
        redis_->Command({"GET", LeaseKey(id)}).value_or("");
    return lease.substr(0, lease.rfind(' '));
  }

  // The records of this world's trades, which the server leaves for the
  // saver.
  std::vector<nanommo::Redis::Entry> Trades() {
    std::vector<nanommo::Redis::Entry> ours;
    for (nanommo::Redis::Entry& trade :
         redis_->Read({"XREAD", "COUNT", "1000", "STREAMS", "trades", "0-0"})
             .value_or(std::vector<nanommo::Redis::Entry>())) {
      if (std::ranges::find(trade.fields, world_) != trade.fields.end()) {
        ours.push_back(std::move(trade));
      }
    }
    return ours;
  }

  // An instance of this world, as its leases name it.
  std::string Instance(const nanommo::Server& server) const {
    return absl::StrCat(world_, "-", server.port());
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
  std::vector<uint32_t> characters_;
  std::map<uint32_t, std::string> names_;
  uint32_t alice_ = 0;
  uint32_t bob_ = 0;
  std::unique_ptr<nanommo::Tokens> tokens_;
  std::unique_ptr<nanommo::Players> players_;
  std::unique_ptr<nanommo::Guilds> guilds_;
  std::unique_ptr<nanommo::Events> events_;
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

TEST_F(ServerTest, HandsItsPlayersOverToTheNextInstance) {
  // The world's next instance, in this process, on a port of its own.
  absl::StatusOr<std::unique_ptr<nanommo::Server>> next =
      nanommo::Server::Create(io_, 0, *server_tls_,
                              std::chrono::milliseconds(50), world_, 1,
                              content_, metrics_, *tokens_, *players_, *guilds_,
                              *events_, *storyteller_);
  ASSERT_TRUE(next.ok()) << next.status();
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  EXPECT_EQ(LeasedTo(alice_), Instance(*server_));

  // Her lease goes to the next instance before she's told to move there.
  EXPECT_EQ(server_->HandOver("localhost", (*next)->port()), 1);
  EXPECT_EQ(LeasedTo(alice_), Instance(**next));
  ASSERT_TRUE(RunUntil([&] { return alice.world().has_move(); }));
  const nanommo::protocol::Move move = alice.world().move();
  EXPECT_EQ(move.port(), (*next)->port());

  // With the move's token, alice plays on in the next instance, and the one
  // before has nobody left once she leaves it, and leaves her lease alone.
  TestPlayer moved(io_, *client_tls_, static_cast<uint16_t>(move.port()),
                   move.token());
  EXPECT_TRUE(RunUntil([&] { return moved.Sees(alice_); }));
  alice.Leave();
  EXPECT_TRUE(RunUntil([&] { return server_->players() == 0; }));
  EXPECT_EQ(LeasedTo(alice_), Instance(**next));
}

TEST_F(ServerTest, ACharacterPlaysOnOneInstanceAtATime) {
  // The world's second instance, which shares its Redis.
  absl::StatusOr<std::unique_ptr<nanommo::Server>> second =
      nanommo::Server::Create(io_, 0, *server_tls_,
                              std::chrono::milliseconds(50), world_, 1,
                              content_, metrics_, *tokens_, *players_, *guilds_,
                              *events_, *storyteller_);
  ASSERT_TRUE(second.ok()) << second.status();
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  // With a token of her own, she can't play on the second as well, as the
  // first has her lease.
  TestPlayer again(io_, *client_tls_, (*second)->port(), Token(alice_));
  EXPECT_TRUE(RunUntil([&] { return again.closed(); }));
  EXPECT_EQ((*second)->players(), 0);
  EXPECT_EQ(server_->players(), 1);
  EXPECT_EQ(LeasedTo(alice_), Instance(*server_));
}

TEST_F(ServerTest, ACharacterPlaysOnAnotherInstanceOnceItLeavesTheFirst) {
  absl::StatusOr<std::unique_ptr<nanommo::Server>> second =
      nanommo::Server::Create(io_, 0, *server_tls_,
                              std::chrono::milliseconds(50), world_, 1,
                              content_, metrics_, *tokens_, *players_, *guilds_,
                              *events_, *storyteller_);
  ASSERT_TRUE(second.ok()) << second.status();
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));
  alice.Leave();
  ASSERT_TRUE(RunUntil([&] { return server_->players() == 0; }));

  // Her lease went as she left, so the second takes her at once, and not ten
  // seconds later, once it would have run out.
  TestPlayer later(io_, *client_tls_, (*second)->port(), Token(alice_));
  EXPECT_TRUE(RunUntil([&] { return later.Sees(alice_); }));
  EXPECT_EQ(LeasedTo(alice_), Instance(**second));
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

TEST_F(ServerTest, AnInstanceDropsAPlayerWhoseLeaseItsLost) {
  // Each joins with the kit: a sword in hand, and three potions.
  const nanommo::Content gear{.items = nanommo::testing::Gear()};
  std::vector<std::unique_ptr<nanommo::Server>> instances;
  for (int i = 0; i < 2; ++i) {
    absl::StatusOr<std::unique_ptr<nanommo::Server>> instance =
        nanommo::Server::Create(io_, 0, *server_tls_,
                                std::chrono::milliseconds(50), world_, 1, gear,
                                metrics_, *tokens_, *players_, *guilds_,
                                *events_, *storyteller_);
    ASSERT_TRUE(instance.ok()) << instance.status();
    instances.push_back(*std::move(instance));
  }
  nanommo::Server& first = *instances[0];
  nanommo::Server& second = *instances[1];
  TestPlayer alice(io_, *client_tls_, first.port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.bag().revision() > 0; }));

  // The first stalls for longer than a lease, which runs out, and the second
  // takes her, with a higher fencing number.
  ASSERT_TRUE(redis_->Command({"DEL", LeaseKey(alice_)}).ok());
  TestPlayer again(io_, *client_tls_, second.port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return again.bag().revision() > 0; }));
  EXPECT_EQ(LeasedTo(alice_), Instance(second));

  // On the first, she puts her sword in her bag, which it can't save over
  // what the second saves, so it drops her.
  nanommo::protocol::Input stow;
  nanommo::protocol::ItemMove* sword = stow.mutable_item()->mutable_move();
  *sword->mutable_from() = nanommo::Worn(nanommo::items::SLOT_MAIN_HAND);
  *sword->mutable_to() = nanommo::InBag(1);
  alice.Send(stow);
  ASSERT_TRUE(RunUntil([&] { return alice.closed(); }));
  EXPECT_EQ(first.players(), 0);
  EXPECT_EQ(second.players(), 1);
  EXPECT_EQ(LeasedTo(alice_), Instance(second));

  // Redis has her sword in her hand, as the second has it.
  const absl::StatusOr<std::vector<std::vector<std::string>>> kept =
      redis_->Pipeline(
          {{"HGET", absl::StrCat("player:", world_, ":", alice_), "items"}});
  ASSERT_TRUE(kept.ok()) << kept.status();
  nanommo::protocol::Inventory saved;
  ASSERT_TRUE(saved.ParseFromString(kept->front().front()));
  EXPECT_TRUE(saved.worn().contains(nanommo::items::SLOT_MAIN_HAND));
  EXPECT_FALSE(saved.bag().contains(1));
}

TEST_F(ServerTest, KeepsAPlayerInADungeonOutsideIt) {
  // Alice left the game standing in the Meadows' gate to the Crypt.
  constexpr int kCrypt = 2;
  const nanommo::Gate& gate =
      *std::ranges::find(nanommo::kGates, kCrypt, &nanommo::Gate::to);
  const std::string key = absl::StrCat("player:", world_, ":", alice_);
  ASSERT_TRUE(
      redis_
          ->Pipeline({{"HSET", key, "x", absl::StrCat(gate.place.x), "z",
                       absl::StrCat(gate.place.z), "experience", "0", "health",
                       "", "land", "Meadows", "weapon", "", "skills", ""}})
          .ok());

  // She comes back in it, and goes through, into the Crypt, while what's kept
  // of her is outside it.
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.world().land() == kCrypt; }));
  const nanommo::Place outside = nanommo::Arrival(nanommo::Twin(gate), 0.0f);
  const absl::StatusOr<std::vector<std::vector<std::string>>> kept =
      redis_->Pipeline({{"HMGET", key, "land", "x", "z"}});
  ASSERT_TRUE(kept.ok()) << kept.status();
  EXPECT_EQ(kept->front(),
            (std::vector<std::string>{"Meadows", absl::StrCat(outside.x),
                                      absl::StrCat(outside.z)}));

  // So when she plays again, she's in the Meadows, out of the gate.
  alice.Leave();
  ASSERT_TRUE(RunUntil([&] { return server_->players() == 0; }));
  TestPlayer later(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return later.Sees(alice_); }));
  EXPECT_EQ(later.world().land(), 0);
  for (const nanommo::protocol::Player& player : later.world().players()) {
    if (player.id() == alice_) {
      EXPECT_FLOAT_EQ(player.x(), outside.x);
      EXPECT_FLOAT_EQ(player.z(), outside.z);
    }
  }
}

TEST_F(ServerTest, SendsEachPlayerTheirBagAsItChanges) {
  const nanommo::Content gear{.items = nanommo::testing::Gear()};
  absl::StatusOr<std::unique_ptr<nanommo::Server>> armed =
      nanommo::Server::Create(
          io_, 0, *server_tls_, std::chrono::milliseconds(50), world_, 1, gear,
          metrics_, *tokens_, *players_, *guilds_, *events_, *storyteller_);
  ASSERT_TRUE(armed.ok()) << armed.status();
  TestPlayer alice(io_, *client_tls_, (*armed)->port(), Token(alice_));

  // She joins with the kit: the sword in hand, and three potions.
  ASSERT_TRUE(RunUntil([&] { return alice.bag().revision() > 0; }));
  EXPECT_EQ(alice.bag().worn().at(nanommo::items::SLOT_MAIN_HAND).item(), 1u);
  EXPECT_EQ(alice.bag().bag().at(0).count(), 3u);

  // She moves two of the potions to another slot, and the bag comes again.
  nanommo::protocol::Input move;
  nanommo::protocol::ItemMove* potions = move.mutable_item()->mutable_move();
  *potions->mutable_from() = nanommo::InBag(0);
  *potions->mutable_to() = nanommo::InBag(4);
  potions->set_count(2);
  alice.Send(move);
  ASSERT_TRUE(RunUntil([&] { return alice.bag().bag().contains(4); }));
  EXPECT_EQ(alice.bag().bag().at(4).count(), 2u);
  EXPECT_EQ(alice.bag().bag().at(0).count(), 1u);

  // What she can't do comes back as a line from nobody.
  nanommo::protocol::Input drink;
  drink.mutable_item()->set_use(4);
  alice.Send(drink);
  ASSERT_TRUE(RunUntil([&] { return !alice.notices().empty(); }));
  EXPECT_EQ(alice.notices().back(), "You're at full health already.");

  // And Redis has the bag she has.
  const absl::StatusOr<std::vector<std::vector<std::string>>> kept =
      redis_->Pipeline(
          {{"HGET", absl::StrCat("player:", world_, ":", alice_), "items"}});
  ASSERT_TRUE(kept.ok()) << kept.status();
  nanommo::protocol::Inventory saved;
  ASSERT_TRUE(saved.ParseFromString(kept->front().front()));
  EXPECT_EQ(saved.revision(), alice.bag().revision());
}

TEST_F(ServerTest, SendsEachPlayerTheirQuestsAsTheyChange) {
  // A guard where new players start, who asks for bones.
  nanommo::Content content{.items = nanommo::testing::Gear(),
                           .quests = nanommo::testing::Bones()};
  content.npcs = *nanommo::ParseNpcs(R"pb(
    kinds {
      id: 1
      name: "Guard"
      look { skin: 0xF1C49CFF }
    }
    spawns { kind: 1 land: "Meadows" x: 1 z: 0 }
  )pb");
  absl::StatusOr<std::unique_ptr<nanommo::Server>> guarded =
      nanommo::Server::Create(io_, 0, *server_tls_,
                              std::chrono::milliseconds(50), world_, 1, content,
                              metrics_, *tokens_, *players_, *guilds_, *events_,
                              *storyteller_);
  ASSERT_TRUE(guarded.ok()) << guarded.status();
  TestPlayer alice(io_, *client_tls_, (*guarded)->port(), Token(alice_));

  // Her quests come as she joins, on none.
  ASSERT_TRUE(RunUntil([&] { return alice.quests().has_value(); }));
  EXPECT_TRUE(alice.quests()->on().empty());

  // She takes the guard's, and they come again.
  nanommo::protocol::Input take;
  take.mutable_take()->set_npc(1);
  take.mutable_take()->set_quest(1);
  alice.Send(take);
  ASSERT_TRUE(RunUntil([&] { return alice.quests()->on().contains(1); }));
  EXPECT_EQ(alice.quests()->on().at(1), 0u);

  // What she can't do comes back as a line from nobody.
  alice.Send(take);
  ASSERT_TRUE(RunUntil([&] { return !alice.notices().empty(); }));
  EXPECT_EQ(alice.notices().back(), "You're on Bones already.");
}

TEST_F(ServerTest, ShowsEachHitToThePlayersNearIt) {
  // A skeleton a step from where new players start, and a sword for each.
  nanommo::Content content{.items = nanommo::testing::Sword(),
                           .skills = nanommo::testing::WithStrike({})};
  content.monsters.kinds[1] = nanommo::testing::Skeleton();
  nanommo::monsters::Spawn spawn;
  spawn.set_kind(1);
  spawn.set_land("Meadows");
  spawn.set_x(1.0f);
  content.monsters.spawns.push_back(spawn);
  absl::StatusOr<std::unique_ptr<nanommo::Server>> haunted =
      nanommo::Server::Create(io_, 0, *server_tls_,
                              std::chrono::milliseconds(50), world_, 1, content,
                              metrics_, *tokens_, *players_, *guilds_, *events_,
                              *storyteller_);
  ASSERT_TRUE(haunted.ok()) << haunted.status();
  TestPlayer alice(io_, *client_tls_, (*haunted)->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, (*haunted)->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(bob_) && bob.Sees(alice_); }));

  // alice swings, and bob, beside her, sees her hit land.
  nanommo::protocol::Input swing;
  swing.set_attack(true);
  swing.set_target(1);
  alice.Send(swing);
  const auto hit = [](const TestPlayer& player) {
    return std::ranges::find(player.events(), nanommo::protocol::EVENT_HIT,
                             &nanommo::protocol::Event::kind);
  };
  ASSERT_TRUE(RunUntil([&] { return hit(bob) != bob.events().end(); }));
  EXPECT_EQ(hit(bob)->player(), alice_);
  EXPECT_EQ(hit(bob)->monster(), 1u);
  EXPECT_EQ(hit(bob)->amount(), 15u);
  EXPECT_NE(hit(alice), alice.events().end());
}

TEST_F(ServerTest, FoundsAGuildFromTheGame) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  // Each hears they're in no guild as they join.
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  EXPECT_EQ(alice.guild()->name(), "");

  nanommo::protocol::Input found;
  found.mutable_guild()->set_create("Iron  Wolves");
  alice.Send(found);
  ASSERT_TRUE(RunUntil([&] { return !alice.notices().empty(); }));
  EXPECT_EQ(alice.notices().back(),
            "Guild names are letters, with one space between words.");

  found.mutable_guild()->set_create("Iron Wolves");
  alice.Send(found);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  EXPECT_EQ(alice.guild()->rank(), nanommo::protocol::RANK_LEADER);

  found.mutable_guild()->set_create("iron wolves");
  bob.Send(found);
  ASSERT_TRUE(RunUntil([&] { return !bob.notices().empty(); }));
  EXPECT_EQ(
      bob.notices().back(),
      absl::StrCat("There's a guild called that in ", world_, " already."));
  EXPECT_EQ(bob.guild()->name(), "");
}

TEST_F(ServerTest, OthersSeeTheGuildWithYourName) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return bob.Sees(alice_); }));

  nanommo::protocol::Input found;
  found.mutable_guild()->set_create("Iron Wolves");
  alice.Send(found);
  // It comes with her name, in the world sent in full once a second.
  EXPECT_TRUE(RunUntil([&] {
    return std::ranges::any_of(
        bob.world().players(), [&](const nanommo::protocol::Player& player) {
          return player.id() == alice_ && player.guild() == "Iron Wolves";
        });
  }));
}

TEST_F(ServerTest, AskedPlayersJoin) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  nanommo::protocol::Input found;
  found.mutable_guild()->set_create("Iron Wolves");
  alice.Send(found);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));

  // By their name, whatever its case.
  nanommo::protocol::Input ask;
  ask.mutable_guild()->set_invite(absl::AsciiStrToUpper(names_[bob_]));
  alice.Send(ask);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  EXPECT_EQ(bob.invite()->guild(), "Iron Wolves");
  EXPECT_EQ(bob.invite()->from(), names_[alice_]);

  nanommo::protocol::Input accept;
  accept.mutable_guild()->set_accept(true);
  bob.Send(accept);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));
  EXPECT_EQ(bob.guild()->rank(), nanommo::protocol::RANK_MEMBER);
  const std::string joined = absl::StrCat(names_[bob_], " joined Iron Wolves.");
  EXPECT_TRUE(RunUntil([&] {
    return !alice.notices().empty() && alice.notices().back() == joined;
  }));
}

TEST_F(ServerTest, GuildChatReachesTheGuildAlone) {
  const uint32_t carol_id = NewAccount();
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  TestPlayer carol(io_, *client_tls_, server_->port(), Token(carol_id));
  ASSERT_TRUE(
      RunUntil([&] { return alice.guild() && bob.guild() && carol.guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));

  input.mutable_guild()->set_say("meet at the Crypt gate");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return !bob.heard().empty(); }));
  EXPECT_EQ(bob.heard().back().name(), names_[alice_]);
  EXPECT_EQ(bob.heard().back().text(), "meet at the Crypt gate");
  EXPECT_EQ(bob.heard().back().channel(), nanommo::protocol::CHANNEL_GUILD);
  // Carol hears the world's line alice says after, and not the guild's
  // before it, over the same connection.
  nanommo::protocol::Input world;
  world.set_say("hello all");
  alice.Send(world);
  ASSERT_TRUE(RunUntil([&] { return !carol.heard().empty(); }));
  EXPECT_EQ(carol.heard().size(), 1u);
  EXPECT_EQ(carol.heard().front().text(), "hello all");
}

TEST_F(ServerTest, LeavingTakesTheTagOffAndHandsTheGuildOn) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));

  input.mutable_guild()->set_leave(true);
  alice.Send(input);
  EXPECT_TRUE(RunUntil([&] {
    return alice.guild()->name().empty() &&
           bob.guild()->rank() == nanommo::protocol::RANK_LEADER;
  }));
  const std::string news = absl::StrCat(names_[alice_], " left Iron Wolves. ",
                                        names_[bob_], " leads it now.");
  EXPECT_TRUE(RunUntil(
      [&] { return !bob.notices().empty() && bob.notices().back() == news; }));
}

TEST_F(ServerTest, TheRosterShowsWhosPlaying) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  auto bob = std::make_unique<TestPlayer>(io_, *client_tls_, server_->port(),
                                          Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob->guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob->invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob->Send(input);
  ASSERT_TRUE(RunUntil([&] {
    return alice.guild()->members_size() == 2 &&
           alice.guild()->members(1).online();
  }));
  EXPECT_EQ(alice.guild()->members(1).id(), bob_);

  // Bob goes, and alice's roster shows him offline.
  bob->Leave();
  EXPECT_TRUE(RunUntil([&] {
    return alice.guild()->members_size() == 2 &&
           !alice.guild()->members(1).online();
  }));
}

TEST_F(ServerTest, TheLeaderMakesAnOfficer) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));

  input.mutable_guild()->set_promote(bob_);
  alice.Send(input);
  EXPECT_TRUE(RunUntil([&] {
    return bob.guild()->rank() == nanommo::protocol::RANK_OFFICER &&
           alice.guild()->members_size() == 2 &&
           alice.guild()->members(1).rank() == nanommo::protocol::RANK_OFFICER;
  }));
  const std::string news = absl::StrCat(names_[bob_], " is an officer now.");
  EXPECT_TRUE(RunUntil(
      [&] { return !bob.notices().empty() && bob.notices().back() == news; }));
}

TEST_F(ServerTest, TheLeaderRemovesAMember) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));

  input.mutable_guild()->set_kick(bob_);
  alice.Send(input);
  EXPECT_TRUE(RunUntil([&] {
    return bob.guild()->name().empty() && alice.guild()->members_size() == 1;
  }));
  EXPECT_TRUE(RunUntil([&] {
    return !bob.notices().empty() &&
           bob.notices().back() == "You were removed from Iron Wolves.";
  }));
}

TEST_F(ServerTest, DisbandingTakesEveryonesTagOff) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  ASSERT_TRUE(RunUntil([&] { return alice.guild() && bob.guild(); }));
  nanommo::protocol::Input input;
  input.mutable_guild()->set_create("Iron Wolves");
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return alice.guild()->name() == "Iron Wolves"; }));
  input.mutable_guild()->set_invite(names_[bob_]);
  alice.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.invite().has_value(); }));
  input.mutable_guild()->set_accept(true);
  bob.Send(input);
  ASSERT_TRUE(RunUntil([&] { return bob.guild()->name() == "Iron Wolves"; }));

  input.mutable_guild()->set_disband(true);
  alice.Send(input);
  EXPECT_TRUE(RunUntil([&] {
    return alice.guild()->name().empty() && bob.guild()->name().empty();
  }));
  const std::string news =
      absl::StrCat(names_[alice_], " disbanded Iron Wolves.");
  EXPECT_TRUE(RunUntil(
      [&] { return !bob.notices().empty() && bob.notices().back() == news; }));
}

TEST_F(ServerTest, TwoPlayersTrade) {
  // Each joins with the kit: a sword in hand, and three potions.
  const nanommo::Content gear{.items = nanommo::testing::Gear()};
  absl::StatusOr<std::unique_ptr<nanommo::Server>> market =
      nanommo::Server::Create(
          io_, 0, *server_tls_, std::chrono::milliseconds(50), world_, 1, gear,
          metrics_, *tokens_, *players_, *guilds_, *events_, *storyteller_);
  ASSERT_TRUE(market.ok()) << market.status();
  TestPlayer alice(io_, *client_tls_, (*market)->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, (*market)->port(), Token(bob_));
  ASSERT_TRUE(RunUntil(
      [&] { return alice.bag().revision() > 0 && bob.bag().revision() > 0; }));

  // Alice asks bob, by his name, and he asks her back.
  nanommo::protocol::Input ask;
  ask.mutable_trade()->set_ask(names_[bob_]);
  alice.Send(ask);
  const std::string asked =
      absl::StrCat(names_[alice_], " asks to trade. Type /trade ",
                   names_[alice_], " to say yes.");
  ASSERT_TRUE(RunUntil(
      [&] { return !bob.notices().empty() && bob.notices().back() == asked; }));
  ask.mutable_trade()->set_ask(names_[alice_]);
  bob.Send(ask);
  ASSERT_TRUE(RunUntil([&] { return alice.trade() && bob.trade(); }));
  EXPECT_EQ(alice.trade()->partner(), names_[bob_]);
  EXPECT_EQ(bob.trade()->partner(), names_[alice_]);

  // She offers her potions, and he sees them.
  nanommo::protocol::Input offer;
  offer.mutable_trade()->mutable_offer()->add_bag(0);
  alice.Send(offer);
  ASSERT_TRUE(RunUntil([&] {
    return bob.trade()->theirs().bag().contains(0) &&
           alice.trade()->yours().bag().contains(0);
  }));
  EXPECT_EQ(bob.trade()->theirs().bag().at(0).count(), 3u);

  // Once both are ready, for the offers they've seen, the potions go across,
  // and it's over for both.
  nanommo::protocol::Input ready;
  ready.mutable_trade()->set_ready(true);
  ready.mutable_trade()->set_revision(alice.trade()->revision());
  alice.Send(ready);
  ASSERT_TRUE(RunUntil([&] { return bob.trade()->theirs().ready(); }));
  ready.mutable_trade()->set_revision(bob.trade()->revision());
  bob.Send(ready);
  ASSERT_TRUE(RunUntil([&] {
    return bob.bag().bag().contains(0) && bob.bag().bag().at(0).count() == 6u &&
           !alice.bag().bag().contains(0);
  }));
  EXPECT_TRUE(RunUntil([&] {
    return alice.trade()->partner().empty() && bob.trade()->partner().empty();
  }));
}

TEST_F(ServerTest, ATradeAndALeaveLeaveOneSword) {
  // Each joins with the kit: a sword in hand, and three potions.
  const nanommo::Content gear{.items = nanommo::testing::Gear()};
  absl::StatusOr<std::unique_ptr<nanommo::Server>> market =
      nanommo::Server::Create(
          io_, 0, *server_tls_, std::chrono::milliseconds(50), world_, 1, gear,
          metrics_, *tokens_, *players_, *guilds_, *events_, *storyteller_);
  ASSERT_TRUE(market.ok()) << market.status();
  TestPlayer alice(io_, *client_tls_, (*market)->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, (*market)->port(), Token(bob_));
  // The swords in a bag, and worn.
  const auto swords = [](const nanommo::protocol::Inventory& bag) {
    int count = 0;
    for (const auto& [slot, stack] : bag.bag()) {
      count += stack.item() == 1u ? 1 : 0;
    }
    for (const auto& [slot, stack] : bag.worn()) {
      count += stack.item() == 1u ? 1 : 0;
    }
    return count;
  };
  ASSERT_TRUE(RunUntil(
      [&] { return swords(alice.bag()) == 1 && swords(bob.bag()) == 1; }));
  const uint64_t serial =
      alice.bag().worn().at(nanommo::items::SLOT_MAIN_HAND).serial();

  // Alice puts her sword in her bag, and they open a trade.
  nanommo::protocol::Input stow;
  nanommo::protocol::ItemMove* sword = stow.mutable_item()->mutable_move();
  *sword->mutable_from() = nanommo::Worn(nanommo::items::SLOT_MAIN_HAND);
  *sword->mutable_to() = nanommo::InBag(1);
  alice.Send(stow);
  ASSERT_TRUE(RunUntil([&] { return alice.bag().bag().contains(1); }));
  nanommo::protocol::Input ask;
  ask.mutable_trade()->set_ask(names_[bob_]);
  alice.Send(ask);
  ask.mutable_trade()->set_ask(names_[alice_]);
  bob.Send(ask);
  ASSERT_TRUE(RunUntil([&] { return alice.trade() && bob.trade(); }));

  // She offers it, and once bob's ready for it, she's ready too, and gone
  // before the next tick.
  nanommo::protocol::Input offer;
  offer.mutable_trade()->mutable_offer()->add_bag(1);
  alice.Send(offer);
  ASSERT_TRUE(
      RunUntil([&] { return bob.trade()->theirs().bag().contains(1); }));
  nanommo::protocol::Input ready;
  ready.mutable_trade()->set_ready(true);
  ready.mutable_trade()->set_revision(bob.trade()->revision());
  bob.Send(ready);
  ASSERT_TRUE(RunUntil([&] { return alice.trade()->theirs().ready(); }));
  ready.mutable_trade()->set_revision(alice.trade()->revision());
  alice.SendAndLeave(ready);
  ASSERT_TRUE(RunUntil(
      [&] { return (*market)->players() == 1 && swords(bob.bag()) == 2; }));
  // The trade's on record, with her sword's serial, as she gave it.
  const std::vector<nanommo::Redis::Entry> trades = Trades();
  ASSERT_EQ(trades.size(), 1u);
  const std::vector<std::string>& fields = trades.front().fields;
  const auto field = [&fields](std::string_view name) {
    const auto found = std::ranges::find(fields, name);
    return found != fields.end() && found + 1 != fields.end() ? *(found + 1)
                                                              : "";
  };
  EXPECT_EQ(field("player"), absl::StrCat(alice_));
  EXPECT_EQ(field("partner"), absl::StrCat(bob_));
  EXPECT_TRUE(absl::StrContains(field("player_gave"), absl::StrCat(serial)));

  // Bob has her sword, and she comes back without it.
  TestPlayer back(io_, *client_tls_, (*market)->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return back.bag().revision() > 0; }));
  EXPECT_EQ(swords(back.bag()) + swords(bob.bag()), 2);
}

TEST_F(ServerTest, AppliesNumberedInputsOneATickInOrder) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  // Three at once, as a network that held them back would let them through.
  std::vector<nanommo::protocol::Input> held(3);
  for (uint32_t sequence = 1; sequence <= 3; ++sequence) {
    held[sequence - 1].set_x(1.0f);
    held[sequence - 1].set_sequence(sequence);
  }
  alice.SendTogether(held);
  std::set<uint32_t> seen;
  EXPECT_TRUE(RunUntil([&] {
    seen.insert(alice.world().acked());
    return alice.world().acked() == 3;
  }));
  // Each tick took one, so the worlds in between said 1 and 2.
  EXPECT_TRUE(seen.contains(1) && seen.contains(2));
}

TEST_F(ServerTest, DrainsAQueueABurstLeftFull) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  // Three at once, and then one each world, as a client keeps up after a slow
  // frame: two wait behind each new one, until the queue drains them.
  std::vector<nanommo::protocol::Input> held(3);
  for (uint32_t sequence = 1; sequence <= 3; ++sequence) {
    held[sequence - 1].set_sequence(sequence);
  }
  alice.SendTogether(held);
  uint32_t sequence = 3;
  int64_t last = alice.world().sent();
  EXPECT_TRUE(RunUntil([&] {
    if (alice.world().sent() == last) {
      return false;
    }
    last = alice.world().sent();
    // Level: the world applied the newest we'd sent before it.
    if (sequence > 3 && alice.world().acked() == sequence) {
      return true;
    }
    nanommo::protocol::Input next;
    next.set_sequence(++sequence);
    alice.Send(next);
    return false;
  }));
}

TEST_F(ServerTest, DrainsNothingFromAClientThatKeepsUp) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  // One each world for two seconds: each is applied, and none is dropped.
  uint32_t sequence = 0;
  int64_t last = alice.world().sent();
  std::set<uint32_t> seen;
  EXPECT_TRUE(RunUntil([&] {
    if (alice.world().sent() == last) {
      return false;
    }
    last = alice.world().sent();
    seen.insert(alice.world().acked());
    if (sequence == 40) {
      return true;
    }
    nanommo::protocol::Input next;
    next.set_sequence(++sequence);
    alice.Send(next);
    return false;
  }));
  for (uint32_t applied = 1; applied < 40; ++applied) {
    EXPECT_TRUE(seen.contains(applied)) << applied;
  }
}

TEST_F(ServerTest, MoreInputsThanTicksDontRunFaster) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));
  const auto place = [&] {
    for (const nanommo::protocol::Player& player : alice.world().players()) {
      if (player.id() == alice_) {
        return std::pair{player.x(), player.z()};
      }
    }
    return std::pair{0.0f, 0.0f};
  };

  // Three running inputs each world, for two seconds, where the client sends
  // one a tick: the server still applies one a tick.
  const auto [from_x, from_z] = place();
  const int64_t from = alice.world().sent();
  int64_t last = from;
  uint32_t sequence = 0;
  EXPECT_TRUE(RunUntil([&] {
    if (alice.world().sent() == last) {
      return false;
    }
    last = alice.world().sent();
    if (last - from >= 2'000'000) {
      return true;
    }
    std::vector<nanommo::protocol::Input> three(3);
    for (nanommo::protocol::Input& input : three) {
      input.set_x(1.0f);
      input.set_run(true);
      input.set_sequence(++sequence);
    }
    alice.SendTogether(three);
    return false;
  }));
  const auto [to_x, to_z] = place();
  const double speed = std::hypot(to_x - from_x, to_z - from_z) /
                       (static_cast<double>(last - from) / 1e6);
  // It ran, and no faster than running.
  EXPECT_GT(speed, nanommo::kRunSpeed * 0.5);
  EXPECT_LE(speed, nanommo::kRunSpeed * 1.05);
}

TEST_F(ServerTest, CatchesUpWithAClientItFellBehind) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  ASSERT_TRUE(RunUntil([&] { return alice.Sees(alice_); }));

  // Five held back and let through together: the server skips the oldest
  // while more than three wait, and is level again in four ticks.
  std::vector<nanommo::protocol::Input> held(5);
  for (uint32_t sequence = 1; sequence <= 5; ++sequence) {
    held[sequence - 1].set_sequence(sequence);
  }
  alice.SendTogether(held);
  std::set<uint32_t> seen;
  EXPECT_TRUE(RunUntil([&] {
    seen.insert(alice.world().acked());
    return alice.world().acked() == 5;
  }));
  EXPECT_FALSE(seen.contains(1));
}

TEST_F(ServerTest, LeavesOutWhatHasntChangedSinceTheWorldTheClientHas) {
  TestPlayer alice(io_, *client_tls_, server_->port(), Token(alice_));
  TestPlayer bob(io_, *client_tls_, server_->port(), Token(bob_));
  // A world without names, as most are: names come once a second.
  ASSERT_TRUE(RunUntil([&] {
    return alice.Sees(bob_) && alice.world().players(0).name().empty();
  }));

  // Both stand still, and alice says she has that world whole.
  nanommo::protocol::Input input;
  input.set_seen(alice.world().sent());
  alice.Send(input);
  // Then bob comes to her by his id alone.
  EXPECT_TRUE(RunUntil([&] {
    const auto& still = alice.world().still_players();
    return std::ranges::find(still, bob_) != still.end() && !alice.Sees(bob_);
  }));
}
