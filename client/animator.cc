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

}  // namespace

std::vector<Figure> Animator::Update(const protocol::World& world,
                                     float seconds) {
  std::map<uint32_t, Figure> next;
  std::vector<Figure> figures;
  for (const protocol::Player& player : world.players()) {
    const Vector3 target{player.x(), 0.0f, player.z()};
    const auto found = figures_.find(player.id());
    Figure figure = found != figures_.end() ? found->second
                                            : Figure{.position = target,
                                                     .yaw = player.yaw(),
                                                     .from = player.motion(),
                                                     .to = player.motion()};
    figure.id = player.id();
    figure.name = player.name();
    figure.you = player.id() == world.you();

    figure.from_time += seconds;
    figure.to_time += seconds;
    if (player.motion() != figure.to) {
      figure.from = figure.to;
      figure.from_time = figure.to_time;
      figure.to = player.motion();
      figure.to_time = 0.0f;
      figure.blend = 0.0f;
    }
    figure.blend = std::min(figure.blend + seconds / kBlendTime, 1.0f);

    const float follow = 1.0f - std::exp(-kFollowRate * seconds);
    figure.position = Vector3Lerp(figure.position, target, follow);
    figure.yaw = Turn(figure.yaw, player.yaw(), kTurnRate * seconds);

    next[player.id()] = figure;
    figures.push_back(std::move(figure));
  }
  figures_ = std::move(next);
  return figures;
}

}  // namespace nanommo
