#include "client/soundboard.h"

#include <memory>

namespace nanommo {

namespace {

// Steps vary more than skills, as we hear far more of them.
constexpr float kStepPitch = 0.08f;
constexpr float kStepVolume = 0.3f;
constexpr float kSkillPitch = 0.04f;
constexpr float kSkillVolume = 0.1f;

// Steps sit under everything else.
constexpr float kStepLevel = 0.6f;

}  // namespace

Soundboard::Soundboard(const AudioDevice& device, const SkillTable& skills)
    : skill_variety_(1, kSkillPitch, kSkillVolume) {
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
  for (const auto& [id, skill] : skills.by_id) {
    if (skill.look().fx().empty()) {
      continue;
    }
    for (const bool target : {false, true}) {
      for (int take = 0; take < kSkillTakes; ++take) {
        skills_[{id, target}].push_back(Clip::Load(
            device,
            SkillSound(skill.look(), static_cast<uint32_t>(take + 1), target),
            2));
      }
    }
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

void Soundboard::Skill(uint32_t skill, bool target, Heard heard) {
  const auto takes = skills_.find({skill, target});
  if (takes != skills_.end()) {
    Play(takes->second, skill_variety_, heard);
  }
}

}  // namespace nanommo
