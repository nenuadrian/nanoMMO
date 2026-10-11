#include "protocol/parts.h"

#include <algorithm>
#include <vector>

namespace nanommo {

namespace {

// Room for the fields besides the players and the monsters: sent, part,
// parts, and the client's own you, experience and next level.
constexpr std::size_t kRoom = 42;

}  // namespace

std::vector<protocol::World> Split(const protocol::World& world,
                                   std::size_t max_size) {
  std::vector<protocol::World> parts(1);
  std::size_t size = 0;
  const auto part_for = [&](std::size_t bytes) -> protocol::World& {
    const bool empty =
        parts.back().players_size() == 0 && parts.back().monsters_size() == 0;
    if (!empty && size + bytes + kRoom > max_size) {
      parts.emplace_back();
      size = 0;
    }
    size += bytes;
    return parts.back();
  };
  // The 3 is a byte for the field's number, and up to two for its size.
  for (const protocol::Player& player : world.players()) {
    *part_for(player.ByteSizeLong() + 3).add_players() = player;
  }
  for (const protocol::Monster& monster : world.monsters()) {
    *part_for(monster.ByteSizeLong() + 3).add_monsters() = monster;
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
    seen_monsters_.clear();
  }
  you_ = part.you();
  experience_ = part.experience();
  next_level_ = part.next_level();
  arrived_.insert(part.part());
  for (const protocol::Player& player : part.players()) {
    players_[player.id()] = player;
    seen_.insert(player.id());
  }
  for (const protocol::Monster& monster : part.monsters()) {
    monsters_[monster.id()] = monster;
    seen_monsters_.insert(monster.id());
  }
  if (arrived_.size() == std::max(part.parts(), 1u)) {
    std::erase_if(players_, [this](const auto& player) {
      return !seen_.contains(player.first);
    });
    std::erase_if(monsters_, [this](const auto& monster) {
      return !seen_monsters_.contains(monster.first);
    });
  }
}

protocol::World Parts::world() const {
  protocol::World world;
  world.set_you(you_);
  world.set_experience(experience_);
  world.set_next_level(next_level_);
  world.set_sent(sent_);
  for (const auto& [id, player] : players_) {
    *world.add_players() = player;
  }
  for (const auto& [id, monster] : monsters_) {
    *world.add_monsters() = monster;
  }
  return world;
}

}  // namespace nanommo
