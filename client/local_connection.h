#ifndef NANOMMO_CLIENT_LOCAL_CONNECTION_H_
#define NANOMMO_CLIENT_LOCAL_CONNECTION_H_

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "client/link.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "server/inputs.h"
#include "server/interest.h"
#include "server/world.h"

namespace nanommo {

/// A world of the client's own, without a server: the world's library, run
/// in the client at the server's tick, for one player, in the first land.
/// Nothing is saved, and there's nobody else.
class LocalConnection final : public Link {
 public:
  LocalConnection(const Content& content, std::string name);

  void Poll() override;
  void Send(const protocol::Input& input) override;
  bool connected() const override { return true; }
  const protocol::World& world() const override { return shown_; }
  const std::deque<protocol::Chat>& chat() const override { return chat_; }
  int64_t whole() const override { return parts_.whole(); }
  const std::optional<protocol::Move>& move() const override { return move_; }
  const protocol::Inventory& inventory() const override { return inventory_; }
  const protocol::Quests& quests() const override { return quests_; }
  const protocol::Guild& guild() const override { return guild_; }
  const protocol::Trade& trade() const override { return trade_; }
  const protocol::Duel& duel() const override { return duel_; }
  std::vector<protocol::Event> TakeEvents() override;
  std::optional<protocol::GuildInvite> TakeInvite() override;

 private:
  void Tick();
  void Show();
  void Notify(const std::string& text);
  void Say(const std::string& name, const std::string& text);

  static constexpr uint32_t kYou = 1;
  static constexpr uint32_t kSeed = 1;
  static constexpr int kLand = 0;

  std::shared_ptr<const Content> content_;
  World world_;
  Inputs inputs_;
  Cells npc_cells_;
  std::chrono::steady_clock::time_point next_tick_;
  int ticks_ = 0;
  bool in_gate_ = false;
  Parts parts_;
  protocol::World shown_;
  std::deque<protocol::Chat> chat_;
  std::optional<protocol::Move> move_;
  protocol::Inventory inventory_;
  protocol::Quests quests_;
  protocol::Guild guild_;
  protocol::Trade trade_;
  protocol::Duel duel_;
  std::vector<protocol::Event> events_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LOCAL_CONNECTION_H_
