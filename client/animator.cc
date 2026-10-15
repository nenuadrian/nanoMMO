#include "client/animator.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

float Turn(float from, float to, float step) {
  const float gap = std::remainder(to - from, 2.0f * std::numbers::pi_v<float>);
  return from + std::clamp(gap, -step, step);
}

template <typename Unit>
Figure Follow(const std::map<uint32_t, Figure>& figures, const Unit& unit,
              float seconds) {
  const Vector3 target{unit.x(), 0.0f, unit.z()};
  const auto found = figures.find(unit.id());
  const bool back = found != figures.end() &&
                    found->second.to == protocol::MOTION_DEAD &&
                    unit.motion() != protocol::MOTION_DEAD;
  Figure figure = found != figures.end() && !back
                      ? found->second
                      : Figure{.position = target,
                               .yaw = unit.yaw(),
                               .from = unit.motion(),
                               .to = unit.motion()};
  figure.id = unit.id();
  figure.name = unit.name();
  figure.level = static_cast<int>(unit.level());
  figure.health = static_cast<int>(unit.health());
  figure.max_health = static_cast<int>(unit.max_health());

  figure.from_time += seconds;
  figure.to_time += seconds;
  if (unit.motion() != figure.to) {
    figure.from = figure.to;
    figure.from_time = figure.to_time;
    figure.to = unit.motion();
    figure.to_time = 0.0f;
    figure.blend = 0.0f;
  }
  figure.blend = std::min(figure.blend + seconds / Animator::kBlendTime, 1.0f);

  const float follow = 1.0f - std::exp(-Animator::kFollowRate * seconds);
  figure.position = Vector3Lerp(figure.position, target, follow);
  figure.yaw = Turn(figure.yaw, unit.yaw(), Animator::kTurnRate * seconds);
  return figure;
}

// NPCs stand still, so each is where the server says, and only its clip's time
// goes on.
Figure Stand(const std::map<uint32_t, Figure>& figures,
             const protocol::Npc& npc, float seconds) {
  const auto found = figures.find(npc.id());
  Figure figure = found != figures.end() ? found->second : Figure{};
  figure.id = npc.id();
  figure.name = npc.name();
  figure.npc = true;
  figure.color = GetColor(npc.color());
  figure.radius = npc.radius();
  figure.position = {npc.x(), 0.0f, npc.z()};
  figure.yaw = npc.yaw();
  figure.to_time += seconds;
  return figure;
}

}  // namespace

std::vector<Figure> Animator::Update(const protocol::World& world,
                                     float seconds) {
  std::map<uint32_t, Figure> players;
  std::map<uint32_t, Figure> monsters;
  std::map<uint32_t, Figure> npcs;
  std::vector<Figure> figures;
  for (const protocol::Player& player : world.players()) {
    Figure figure = Follow(players_, player, seconds);
    figure.you = player.id() == world.you();
    players[player.id()] = figure;
    figures.push_back(std::move(figure));
  }
  for (const protocol::Monster& monster : world.monsters()) {
    Figure figure = Follow(monsters_, monster, seconds);
    figure.monster = true;
    monsters[monster.id()] = figure;
    figures.push_back(std::move(figure));
  }
  for (const protocol::Npc& npc : world.npcs()) {
    Figure figure = Stand(npcs_, npc, seconds);
    npcs[npc.id()] = figure;
    figures.push_back(std::move(figure));
  }
  // Players who left, and monsters and NPCs that went, drop out here.
  players_ = std::move(players);
  monsters_ = std::move(monsters);
  npcs_ = std::move(npcs);
  return figures;
}

}  // namespace nanommo
