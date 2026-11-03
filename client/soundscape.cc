#include "client/soundscape.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

// Even far to one side, a little reaches the other ear.
constexpr float kWidestPan = 0.8f;

}  // namespace

Heard Hear(Vector3 source, Vector3 ears, Vector3 right) {
  const Vector3 away = {source.x - ears.x, 0.0f, source.z - ears.z};
  const float distance = Vector3Length(away);
  if (distance >= kFar) {
    return {.volume = 0.0f, .pan = 0.0f};
  }
  // Sound spreads out as it goes, so past near, it falls as one over the
  // distance, and it fades out on the way to far.
  const float volume = std::min(1.0f, kNear / distance) *
                       std::min(1.0f, (kFar - distance) / (kFar - kNear));
  // What's right on top of us comes from both sides.
  const float side =
      distance > 0.0f ? Vector3DotProduct(away, right) / distance : 0.0f;
  return {.volume = volume,
          .pan = kWidestPan * side * std::min(distance, 1.0f)};
}

Variety::Variety(uint32_t seed, float pitch_spread, float volume_spread)
    : noise_(seed),
      pitch_spread_(pitch_spread),
      volume_spread_(volume_spread) {}

Variety::Take Variety::Next(int takes) {
  int take = 0;
  if (takes > 1) {
    // One of the others: a step on from the last, by one to all but one.
    const int skip =
        1 + static_cast<int>(noise_.Uniform() * static_cast<float>(takes - 1));
    take = last_ < 0 ? skip % takes : (last_ + skip) % takes;
  }
  last_ = take;
  return {.take = take,
          .pitch = 1.0f + noise_.Between(-pitch_spread_, pitch_spread_),
          .volume = 1.0f - noise_.Between(0.0f, volume_spread_)};
}

std::array<float, 2> Footfalls(const ModelAnimation& clip, int left,
                               int right) {
  std::array<float, 2> footfalls{0.0f, 0.5f};
  // The last frame is the first again.
  const int frames = clip.keyframeCount - 1;
  const int bones = static_cast<int>(clip.boneCount);
  if (frames < 2 || left < 0 || right < 0 || left >= bones || right >= bones) {
    return footfalls;
  }
  const std::array<int, 2> feet = {left, right};
  for (std::size_t foot = 0; foot < feet.size(); ++foot) {
    const auto height = [&](int frame) {
      return clip.keyframePoses[frame][feet[foot]].translation.y;
    };
    float low = height(0);
    float high = height(0);
    for (int frame = 1; frame < frames; ++frame) {
      low = std::min(low, height(frame));
      high = std::max(high, height(frame));
    }
    // Down is within a sixth of the way up from its lowest, and it comes
    // down on the frame it gets there.
    const float down = low + (high - low) / 6.0f;
    for (int frame = 0; frame < frames; ++frame) {
      const int before = (frame + frames - 1) % frames;
      if (height(frame) <= down && height(before) > down) {
        footfalls[foot] =
            static_cast<float>(frame) / static_cast<float>(frames);
        break;
      }
    }
  }
  return footfalls;
}

uint64_t Who(const Figure& figure) {
  const uint64_t kind = figure.monster ? 1 : (figure.npc ? 2 : 0);
  return kind << 32 | figure.id;
}

std::vector<uint64_t> Footsteps::Update(const std::map<uint64_t, Gait>& gaits) {
  std::vector<uint64_t> stepped;
  std::map<uint64_t, float> loops;
  for (const auto& [who, gait] : gaits) {
    loops[who] = gait.loops;
    const auto last = loops_.find(who);
    // A walk that's just started, or started over as a run, has no steps yet.
    if (last == loops_.end() || gait.loops < last->second) {
      continue;
    }
    for (const float footfall : gait.footfalls) {
      // Whether the foot's moment came round again since the last update.
      if (std::floor(gait.loops - footfall) >
          std::floor(last->second - footfall)) {
        stepped.push_back(who);
      }
    }
  }
  loops_ = std::move(loops);
  return stepped;
}

std::vector<Figure> Casts::Update(const std::vector<Figure>& figures) {
  std::vector<Figure> started;
  std::map<uint32_t, std::pair<uint32_t, float>> skills;
  for (const Figure& figure : figures) {
    if (figure.monster || figure.npc || figure.to != protocol::MOTION_SKILL) {
      continue;
    }
    skills[figure.id] = {figure.to_skill, figure.to_time};
    const auto last = skills_.find(figure.id);
    const bool fresh = last == skills_.end() ||
                       last->second.first != figure.to_skill ||
                       figure.to_time < last->second.second;
    if (fresh) {
      started.push_back(figure);
    }
  }
  skills_ = std::move(skills);
  return started;
}

Passage::Cue Passage::Update(bool covering, bool in_world) {
  Cue cue = Cue::kNothing;
  if (covering && !covering_ && in_world_) {
    cue = Cue::kDeparture;
  } else if (!covering && covering_ && in_world) {
    cue = Cue::kArrival;
  }
  covering_ = covering;
  in_world_ = in_world;
  return cue;
}

}  // namespace nanommo
