#ifndef NANOMMO_SERVER_SERVER_H_
#define NANOMMO_SERVER_SERVER_H_

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "server/copies.h"
#include "server/events.h"
#include "server/guilds.h"
#include "server/inputs.h"
#include "server/interest.h"
#include "server/metrics.h"
#include "server/players.h"
#include "server/session.h"
#include "server/storyteller.h"
#include "server/token_bucket.h"
#include "server/tokens.h"
#include "server/world.h"

namespace nanommo {

class Server {
 public:
  /// Starts the server, or returns why it couldn't, such as the port
  /// being taken. Clients connect over TLS, with the context. The name keeps
  /// the world's players apart from other worlds', and the seed makes the
  /// terrain.
  static absl::StatusOr<std::unique_ptr<Server>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      std::chrono::nanoseconds interval, std::string name, uint32_t seed,
      const Content& content, Metrics& metrics, Tokens& tokens,
      Players& players, Guilds& guilds, Events& events,
      Storyteller& storyteller);

  /// In all its lands.
  int players() const;

  /// Hands the players over to the world's next instance, at the host and port
  /// players reach it at: saves them, stops ticking and taking players, hands
  /// their leases to it, and tells each client where to go. Returns how many
  /// it told.
  int HandOver(const std::string& host, uint16_t port);

  /// The port it listens on, over TCP and UDP. Given port zero, Create has the
  /// system pick a free one.
  uint16_t port() const;

 private:
  Server(asio::io_context& io, asio::ssl::context& tls,
         std::chrono::nanoseconds interval, std::string name, uint32_t seed,
         const Content& content, Metrics& metrics, Tokens& tokens,
         Players& players, Guilds& guilds, Events& events,
         Storyteller& storyteller);

  absl::Status Start(uint16_t port);
  /// The World of the copy the player's in, or nullptr.
  World* WorldOf(uint32_t id);
  void Accept();
  uint32_t HandleHello(Session& session, const std::string& bytes);
  void Leave(uint32_t id);
  void Say(uint32_t id, const std::string& text);
  void Talk(uint32_t id, const protocol::Talk& talk);
  void Learn(uint32_t id, const protocol::Learn& learn);
  void TakeQuest(uint32_t id, const protocol::Quest& quest);
  void TurnIn(uint32_t id, const protocol::Quest& quest);
  void Item(uint32_t id, const protocol::ItemAction& action);
  void Trade(uint32_t id, const protocol::TradeAction& action);
  void Duel(uint32_t id, const protocol::DuelAction& action);
  void Guild(uint32_t id, const protocol::GuildAction& action);
  void Found(uint32_t id, const std::string& name);
  /// Asks the player of that name in, if they're playing in this world.
  void Ask(uint32_t id, const std::string& name);
  void Join(uint32_t id);
  void QuitGuild(uint32_t id);
  void SetRank(uint32_t id, uint32_t player, protocol::Rank rank);
  void Kick(uint32_t id, uint32_t player);
  void Disband(uint32_t id);
  /// To the guild's players in this world, at once, over TCP.
  void GuildSay(uint32_t id, const std::string& text);
  /// Sends each of the guild's players here a line from the server.
  void TellGuild(uint32_t guild, const std::string& text);
  /// Sends each of the guild's players here its roster with the next news.
  void RefreshGuild(uint32_t guild);
  /// Tells the player why their guild action failed: in the error's words
  /// when it's about what they asked, and with "try again" when it's MySQL.
  void Refuse(uint32_t id, const absl::Status& why);
  /// Sends the player a line from the server, which has no name.
  void Notify(uint32_t id, const std::string& text,
              protocol::Channel channel = protocol::CHANNEL_WORLD);
  void Receive();
  /// Each player's next numbered input, one a tick, before the world steps.
  void ApplyInputs();
  void Tick();
  /// Everyone who's changed since they were last saved, once a tick.
  void SaveChanged();
  /// Keeps this instance's leases on its players, once a second.
  void RenewLeases();
  /// Those of the players who've changed since they were last saved, into
  /// Redis, as one: everyone once a tick, the two sides of a trade as it's
  /// done, and a player as they leave. Only while this instance has their
  /// leases: it returns the players whose leases it's lost, when it saved
  /// none of them. A trade's record goes in with them.
  std::set<uint32_t> Save(const std::vector<uint32_t>& ids,
                          const std::optional<Traded>& traded = std::nullopt);
  /// Closes the player's session without saving them, as another instance
  /// has them, or what they have here can't be saved, and says why.
  void Drop(uint32_t id, std::string_view why);
  /// What happened in the worlds this tick, into the events stream.
  void RecordEvents(const std::vector<Event>& happened);
  /// To each player near what happened, in their copy, after the world, so
  /// that a client getting the world over TCP has the units it's about first.
  void ShowEvents(const std::vector<Event>& happened);
  void Broadcast();
  void BroadcastCopy(const Copy& copy, bool full);
  /// What goes to the player's client in this world: the pieces of the
  /// players and monsters in view that have changed since the world it has
  /// whole, and the rest by their ids, in still, which is one more piece. It
  /// remembers what it sent, for the next.
  std::vector<std::string_view> Delta(
      uint32_t id, const Copy& copy, const View& view,
      const std::map<uint32_t, std::string>& players,
      const std::map<uint32_t, std::string>& monsters, int64_t sent,
      std::string& still);
  void SendChat();
  /// To each player whose bag has changed since we last sent it.
  void SendBags();
  /// To each player as they join, and as their quests change.
  void SendQuests();
  /// To each player as they join, and as their guild changes.
  void SendGuilds();
  /// To each player whose trade opened, changed or ended this tick.
  void SendTrades();
  /// To each player whose duel was asked for, counted down, started or ended.
  void SendDuels();
  /// The guild's roster from MySQL, with who's playing here, and their level
  /// as it is now.
  std::vector<protocol::GuildMember> Roster(uint32_t guild);
  void SendTo(const asio::ip::udp::endpoint& client,
              std::shared_ptr<const std::string> part, std::string own);

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  asio::ip::udp::socket udp_;
  std::array<char, 1024> datagram_;
  asio::ip::udp::endpoint sender_;
  asio::steady_timer timer_;
  std::chrono::nanoseconds interval_;
  std::string name_;
  /// The instance's id, its world's and its port's, as Consul has it, which
  /// its leases on its players name.
  std::string instance_;
  uint32_t seed_;
  Copies copies_;
  Metrics& metrics_;
  Tokens& tokens_;
  Players& players_;
  Guilds& guilds_;
  Events& events_;
  Storyteller& storyteller_;
  std::map<uint32_t, std::shared_ptr<Session>> sessions_;
  std::map<std::string, uint32_t> udp_tokens_;
  std::map<uint32_t, asio::ip::udp::endpoint> udp_clients_;
  std::map<uint32_t, Progress> saved_;
  /// The lease this instance has on each of its players, as Redis has it.
  std::map<uint32_t, std::string> leases_;
  std::chrono::steady_clock::time_point renewed_;
  int broadcasts_ = 0;
  bool handing_over_ = false;
  /// What the players said since the last tick.
  protocol::World chat_;
  std::map<uint32_t, TokenBucket> chat_limits_;
  std::map<uint32_t, TokenBucket> story_limits_;
  std::map<uint32_t, TokenBucket> item_limits_;
  std::map<uint32_t, TokenBucket> guild_limits_;
  std::map<uint32_t, TokenBucket> duel_limits_;
  /// The guild of each player here who's in one.
  std::map<uint32_t, Membership> members_;
  /// The players whose guild we've to send them.
  std::set<uint32_t> guild_news_;
  /// Each player's numbered inputs that haven't been applied yet, and the
  /// last one that has.
  Inputs inputs_;
  /// What each client was sent in its last worlds, by when each was sent:
  /// the copy of the land it was in, as ids are a land's own, and each
  /// player's and monster's piece, by its hash.
  struct Sent {
    const Copy* copy = nullptr;
    std::map<uint64_t, std::size_t> pieces;
  };
  std::map<uint32_t, std::map<int64_t, Sent>> history_;
  /// The newest world each client has whole, by when it was sent.
  std::map<uint32_t, int64_t> seen_;
  /// The revision of each player's bag we sent them last.
  std::map<uint32_t, uint32_t> sent_bags_;
  /// And of their quests.
  std::map<uint32_t, uint32_t> sent_quests_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SERVER_H_
