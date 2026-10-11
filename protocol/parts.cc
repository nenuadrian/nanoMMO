#include "protocol/parts.h"

#include <algorithm>
#include <vector>

namespace nanommo {

namespace {

// Room for the fields besides the players: sent, part, parts, and the
// client's own you.
constexpr std::size_t kRoom = 32;

}  // namespace

std::vector<protocol::World> Split(const protocol::World& world,
                                   std::size_t max_size) {
  std::vector<protocol::World> parts(1);
  std::size_t size = 0;
  for (const protocol::Player& player : world.players()) {
    const std::size_t bytes = player.ByteSizeLong() + 3;
    if (parts.back().players_size() > 0 && size + bytes + kRoom > max_size) {
      parts.emplace_back();
      size = 0;
    }
    *parts.back().add_players() = player;
    size += bytes;
  }
  for (std::size_t i = 0; i < parts.size(); ++i) {
    parts[i].set_sent(world.sent());
    parts[i].set_part(static_cast<uint32_t>(i));
    parts[i].set_parts(static_cast<uint32_t>(parts.size()));
  }
  return parts;
}

void Parts::Add(const protocol::World& part) {
  if (part.sent() < sent_) {
    return;
  }
  if (part.sent() > sent_) {
    sent_ = part.sent();
    arrived_.clear();
    seen_.clear();
  }
  you_ = part.you();
  arrived_.insert(part.part());
  for (const protocol::Player& player : part.players()) {
    players_[player.id()] = player;
    seen_.insert(player.id());
  }
  if (arrived_.size() == std::max(part.parts(), 1u)) {
    std::erase_if(players_, [this](const auto& player) {
      return !seen_.contains(player.first);
    });
  }
}

protocol::World Parts::world() const {
  protocol::World world;
  world.set_you(you_);
  world.set_sent(sent_);
  for (const auto& [id, player] : players_) {
    *world.add_players() = player;
  }
  return world;
}

}  // namespace nanommo
