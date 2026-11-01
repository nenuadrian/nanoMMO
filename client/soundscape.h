#ifndef NANOMMO_CLIENT_SOUNDSCAPE_H_
#define NANOMMO_CLIENT_SOUNDSCAPE_H_

#include <raylib.h>

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/synth.h"

// When the game's sounds play, and how, kept apart from raylib's audio.
namespace nanommo {

/// How a sound is heard: how loud, from 0 to 1, and from which side, from -1
/// for the left to 1 for the right.
struct Heard {
  float volume = 1.0f;
  float pan = 0.0f;
};

/// In metres: full volume this near, and nothing past far.
constexpr float kNear = 4.0f;
constexpr float kFar = 40.0f;

/// A sound at the source, heard by the player, at the ears, with the camera's
/// right to their right. Past near, it falls as one over the distance, and it
/// fades out to nothing at far.
Heard Hear(Vector3 source, Vector3 ears, Vector3 right);

/// Which take of a sound plays next, and how, so that a sound heard again and
/// again doesn't sound like a machine: not the take that played last, a little
/// higher or lower, and at most a little quieter.
class Variety {
 public:
  struct Take {
    int take = 0;
    /// Where 1 is as it was made, and 2 an octave up.
    float pitch = 1.0f;
    float volume = 1.0f;
  };

  /// The pitch can be off by its spread either way, and the volume down by
  /// its spread.
  Variety(uint32_t seed, float pitch_spread, float volume_spread);

  /// One of that many takes.
  Take Next(int takes);

 private:
  Noise noise_;
  float pitch_spread_;
  float volume_spread_;
  int last_ = -1;
};

/// Where a figure is in its walk or run: how many times its clip has looped,
/// and how far into a loop each foot comes down, from 0 to 1.
struct Gait {
  float loops = 0.0f;
  std::array<float, 2> footfalls{0.0f, 0.5f};
};

/// How far into the clip each foot comes down, from 0 to 1: where each foot's
/// bone, left and right, drops to near its lowest. A clip without them steps
/// at its start and halfway.
std::array<float, 2> Footfalls(const ModelAnimation& clip, int left, int right);

/// Which figure is which, as players, monsters and NPCs each count their ids
/// from one.
uint64_t Who(const Figure& figure);

/// Which figures put a foot down, from how far each has got through its walk
/// or run since the last update.
class Footsteps {
 public:
  /// Those, by Who, from each walking or running figure's gait. A figure
  /// that's left out has stopped, and starts afresh.
  std::vector<uint64_t> Update(const std::map<uint64_t, Gait>& gaits);

 private:
  std::map<uint64_t, float> loops_;
};

/// Which players started a skill since the last update: another skill, or the
/// one before again, as their time in it starts over.
class Casts {
 public:
  std::vector<Figure> Update(const std::vector<Figure>& figures);

 private:
  /// Each player's skill, and their time in it, by their id.
  std::map<uint32_t, std::pair<uint32_t, float>> skills_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SOUNDSCAPE_H_
