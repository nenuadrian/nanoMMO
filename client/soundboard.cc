#include "client/soundboard.h"

#include <memory>

namespace nanommo {

namespace {

// Each step a little higher or lower than it was made, and a little quieter.
constexpr float kStepPitch = 0.08f;
constexpr float kStepVolume = 0.3f;

// Steps sit under everything else.
constexpr float kStepLevel = 0.6f;

}  // namespace

Soundboard::Soundboard(const AudioDevice& device) {
  for (const Ground ground : kGrounds) {
    Takes& takes = steps_[ground];
    for (int take = 0; take < kStepTakes; ++take) {
      // A seed for each take of each ground.
      const auto seed = static_cast<uint32_t>(
          static_cast<int>(ground) * kStepTakes + take + 1);
      takes.push_back(Clip::Load(device, nanommo::Step(ground, seed), 4));
    }
    step_varieties_.emplace(ground, Variety(static_cast<uint32_t>(ground),
                                            kStepPitch, kStepVolume));
  }
}

void Soundboard::Play(const Takes& takes, Variety& variety, Heard heard) {
  if (takes.empty() || heard.volume <= 0.0f) {
    return;
  }
  const Variety::Take take = variety.Next(static_cast<int>(takes.size()));
  takes[take.take]->Play(volume_ * heard.volume * take.volume, heard.pan,
                         take.pitch);
}

void Soundboard::Step(Ground ground, Heard heard) {
  heard.volume *= kStepLevel;
  Play(steps_.at(ground), step_varieties_.at(ground), heard);
}

}  // namespace nanommo
