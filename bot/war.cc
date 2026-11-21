#include "bot/war.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "protocol/nanommo.pb.h"

namespace nanommo {

std::optional<protocol::GuildAction> Enlist(const Side& side,
                                            const protocol::Guild& guild,
                                            std::size_t& next) {
  if (!side.leads) {
    return std::nullopt;
  }
  protocol::GuildAction action;
  if (guild.name().empty()) {
    action.set_create(side.guild);
    return action;
  }
  if (!guild.has_war()) {
    action.set_war(side.enemy);
    return action;
  }
  for (std::size_t tried = 0; tried < side.recruits.size(); ++tried) {
    const std::string& name = side.recruits[next++ % side.recruits.size()];
    if (std::ranges::none_of(guild.members(),
                             [&name](const protocol::GuildMember& member) {
                               return member.name() == name;
                             })) {
      action.set_invite(name);
      return action;
    }
  }
  return std::nullopt;
}

uint32_t NearestEnemy(const Side& side, const protocol::World& seen, float x,
                      float z) {
  uint32_t nearest = 0;
  float best = std::numeric_limits<float>::max();
  for (const protocol::Player& player : seen.players()) {
    const float away = std::hypot(player.x() - x, player.z() - z);
    if (player.guild() == side.enemy &&
        player.motion() != protocol::MOTION_DEAD && away < best) {
      best = away;
      nearest = player.id();
    }
  }
  return nearest;
}

}  // namespace nanommo
