#include "protocol/parts.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

// Room for the fields besides the players and the monsters: sent, part,
// parts, and the client's own you, experience, next level, seed and land, its
// cooldowns, up to ten bytes each for the nine skills a hotbar holds, and the
// skills it knows, a byte each, after two for the field.
constexpr std::size_t kRoom = 48 + 9 * 10 + 2 + 9;

// Names, kinds, levels and weapons come once a second, and protobuf leaves
// out a field that's empty or zero, which no name, level or id is, so we keep
// the last ones we had.
template <typename Unit>
void Update(Unit& known, const Unit& now) {
  Unit merged = now;
  if (merged.level() == 0) {
    merged.set_level(known.level());
  }
  if (merged.max_health() == 0) {
    merged.set_max_health(known.max_health());
  }
  if constexpr (std::is_same_v<Unit, protocol::Player>) {
    if (merged.name().empty()) {
      merged.set_name(known.name());
    }
    if (merged.weapon() == 0) {
      merged.set_weapon(known.weapon());
    }
  } else {
    if (merged.kind() == 0) {
      merged.set_kind(known.kind());
    }
  }
  known = std::move(merged);
}

}  // namespace

std::string Piece(const protocol::Player& player) {
  protocol::World world;
  *world.add_players() = player;
  return world.SerializeAsString();
}

std::string Piece(const protocol::Monster& monster) {
  protocol::World world;
  *world.add_monsters() = monster;
  return world.SerializeAsString();
}

std::string Piece(const protocol::Npc& npc) {
  protocol::World world;
  *world.add_npcs() = npc;
  return world.SerializeAsString();
}

std::vector<std::string> Split(const std::vector<std::string_view>& pieces,
                               int64_t sent, std::size_t max_size) {
  std::vector<std::vector<std::string_view>> groups(1);
  std::size_t size = 0;
  for (const std::string_view piece : pieces) {
    if (!groups.back().empty() && size + piece.size() + kRoom > max_size) {
      groups.emplace_back();
      size = 0;
    }
    groups.back().push_back(piece);
    size += piece.size();
  }
  std::vector<std::string> parts;
  for (std::size_t i = 0; i < groups.size(); ++i) {
    protocol::World header;
    header.set_sent(sent);
    header.set_part(static_cast<uint32_t>(i));
    header.set_parts(static_cast<uint32_t>(groups.size()));
    std::string part = header.SerializeAsString();
    for (const std::string_view piece : groups[i]) {
      part.append(piece);
    }
    parts.push_back(std::move(part));
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
  // Each land has NPCs of its own, which may have the last one's ids.
  if (part.land() != land_) {
    npcs_.clear();
  }
  you_ = part.you();
  experience_ = part.experience();
  next_level_ = part.next_level();
  seed_ = part.seed();
  land_ = part.land();
  cooldowns_ = {part.cooldowns().begin(), part.cooldowns().end()};
  skills_ = {part.skills().begin(), part.skills().end()};
  arrived_.insert(part.part());
  for (const protocol::Player& player : part.players()) {
    Update(players_[player.id()], player);
    seen_.insert(player.id());
  }
  for (const protocol::Monster& monster : part.monsters()) {
    Update(monsters_[monster.id()], monster);
    seen_monsters_.insert(monster.id());
  }
  for (const protocol::Npc& npc : part.npcs()) {
    npcs_[npc.id()] = {.npc = npc, .sent = part.sent()};
  }
  std::erase_if(npcs_, [this](const auto& npc) {
    return std::chrono::microseconds(sent_ - npc.second.sent) > kNpcTimeout;
  });
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
  world.set_seed(seed_);
  world.set_land(land_);
  world.mutable_cooldowns()->insert(cooldowns_.begin(), cooldowns_.end());
  world.mutable_skills()->Add(skills_.begin(), skills_.end());
  world.set_sent(sent_);
  for (const auto& [id, player] : players_) {
    *world.add_players() = player;
  }
  for (const auto& [id, monster] : monsters_) {
    *world.add_monsters() = monster;
  }
  for (const auto& [id, heard] : npcs_) {
    *world.add_npcs() = heard.npc;
  }
  return world;
}

}  // namespace nanommo
