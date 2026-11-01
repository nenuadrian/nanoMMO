#ifndef NANOMMO_CLIENT_SOUNDBOARD_H_
#define NANOMMO_CLIENT_SOUNDBOARD_H_

#include <map>
#include <memory>
#include <vector>

#include "client/audio.h"
#include "client/foley.h"
#include "client/soundscape.h"

namespace nanommo {

/// The game's sounds, each made in a few takes as the game starts, and played
/// a take at a time.
class Soundboard {
 public:
  static constexpr int kStepTakes = 8;

  explicit Soundboard(const AudioDevice& device);

  /// How loud the sounds are, from the settings, from 0 to 1.
  void set_volume(float volume) { volume_ = volume; }

  void Step(Ground ground, Heard heard);

 private:
  using Takes = std::vector<std::unique_ptr<Clip>>;

  void Play(const Takes& takes, Variety& variety, Heard heard);

  float volume_ = 1.0f;
  std::map<Ground, Takes> steps_;
  std::map<Ground, Variety> step_varieties_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SOUNDBOARD_H_
