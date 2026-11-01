#ifndef NANOMMO_CLIENT_SOUNDBOARD_H_
#define NANOMMO_CLIENT_SOUNDBOARD_H_

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "client/audio.h"
#include "client/foley.h"
#include "client/soundscape.h"
#include "content/skills.h"

namespace nanommo {

/// The game's sounds, each made in a few takes as the game starts, and played
/// a take at a time.
class Soundboard {
 public:
  /// Takes of each step, and of each skill.
  static constexpr int kStepTakes = 8;
  static constexpr int kSkillTakes = 3;

  Soundboard(const AudioDevice& device, const SkillTable& skills);

  /// How loud the sounds are, from the settings, from 0 to 1.
  void set_volume(float volume) { volume_ = volume; }

  void Step(Ground ground, Heard heard);

  /// The skill's sound, by the skill's id, with the fx at the target if a
  /// monster was picked.
  void Skill(uint32_t skill, bool target, Heard heard);

  void Departure();

  /// Into the land, by its place in the lands.
  void Arrival(int land);

 private:
  using Takes = std::vector<std::unique_ptr<Clip>>;

  void Play(const Takes& takes, Variety& variety, Heard heard);

  float volume_ = 1.0f;
  std::map<Ground, Takes> steps_;
  std::map<Ground, Variety> step_varieties_;
  /// By the skill's id, and whether a monster was picked.
  std::map<std::pair<uint32_t, bool>, Takes> skills_;
  Variety skill_variety_;
  std::unique_ptr<Clip> departure_;
  std::vector<std::unique_ptr<Clip>> arrivals_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SOUNDBOARD_H_
