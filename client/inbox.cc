#include "client/inbox.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

constexpr std::size_t kChatLines = 100;

}  // namespace

absl::Status Inbox::Take(std::string_view message) {
  protocol::World world;
  if (!world.ParseFromArray(message.data(), static_cast<int>(message.size()))) {
    return absl::DataLossError("couldn't read the server's message");
  }
  // Chat, moves, our bag, our quests, our guild, our trade, our duel and the
  // events come on their own, with nothing of the world in them.
  if (world.has_move()) {
    move_ = world.move();
  } else if (world.has_inventory()) {
    inventory_ = world.inventory();
  } else if (world.has_quests()) {
    quests_ = world.quests();
  } else if (world.has_guild()) {
    guild_ = world.guild();
  } else if (world.has_trade()) {
    trade_ = world.trade();
  } else if (world.has_duel()) {
    duel_ = world.duel();
  } else if (world.has_invite()) {
    invite_ = world.invite();
  } else if (!world.events().empty()) {
    events_.insert(events_.end(), world.events().begin(), world.events().end());
  } else if (world.chat().empty()) {
    Add(world);
  } else {
    chat_.insert(chat_.end(), world.chat().begin(), world.chat().end());
    while (chat_.size() > kChatLines) {
      chat_.pop_front();
    }
  }
  return absl::OkStatus();
}

void Inbox::Add(const protocol::World& part) {
  parts_.Add(part);
  world_ = parts_.world();
}

std::vector<protocol::Event> Inbox::TakeEvents() {
  return std::exchange(events_, {});
}

std::optional<protocol::GuildInvite> Inbox::TakeInvite() {
  return std::exchange(invite_, std::nullopt);
}

}  // namespace nanommo
