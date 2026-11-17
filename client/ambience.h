#ifndef NANOMMO_CLIENT_AMBIENCE_H_
#define NANOMMO_CLIENT_AMBIENCE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "client/synth.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// How loud each of the weather's sounds and the time of day's should be,
/// from nothing to full.
struct Hearing {
  /// Drops on the ground and the leaves, and more of them, harder, in a
  /// storm.
  float rain = 0.0f;
  float downpour = 0.0f;
  /// Drops on the sea, when we're by it.
  float water = 0.0f;
  float wind = 0.0f;
  /// Fire falling: crackling, and hissing where it lands.
  float fire = 0.0f;
  /// The lava, bubbling.
  float lava = 0.0f;
  float crickets = 0.0f;
  float birds = 0.0f;
  float owls = 0.0f;
  /// Water dripping in the Crypt, off the stone.
  float drips = 0.0f;
};

/// What a land sounds like under its sky: the Meadows' rain, wind, birds
/// and crickets, the Ashlands' lava and fire, the Crypt's drips. By the sea,
/// the rain falls on the water. Off, the weather's quiet, and the rest
/// keeps to the time of day.
Hearing HearingOf(int land, const protocol::Sky& sky, bool by_water,
                  bool weather);

/// The weather and the time of day, as sound: a few loops made as the game
/// starts, that play all the time, each as loud as it should be now, and
/// birds, owls, drips and thunder now and then, made as they come.
class Ambience {
 public:
  explicit Ambience(uint32_t seed);

  /// Rain recorded from the real thing, in place of the rain made from
  /// numbers, its two channels as long as each other. It loops, its end
  /// fading into its start, and plays as loud as the one it replaces.
  void UseRain(Samples left, Samples right);

  /// Fire recorded crackling, in place of the fire made from numbers, the
  /// same way.
  void UseFire(Samples left, Samples right);

  /// A gust of wind recorded, which blows now and then over the wind made
  /// from numbers, more often the windier it is.
  void UseGust(const Samples& left, const Samples& right);

  /// Adds the next of the sound into the two channels, as many samples as
  /// they have, while each loop glides to how loud the hearing says.
  void Fill(std::span<float> left, std::span<float> right,
            const Hearing& hearing);

  /// The thunder of a bolt this many metres away, from that side, from -1
  /// for the left to 1 for the right. It comes as long after the flash as
  /// sound takes to come that far.
  void Thunder(float metres, float pan);

 private:
  enum Bed { kRain, kDownpour, kWater, kWind, kFire, kLava, kCrickets, kBeds };

  /// Each loop goes round at its own length, so a long recording and a
  /// short one made from numbers can play side by side.
  struct Loop {
    Samples left;
    Samples right;
    float level = 0.0f;
  };

  /// A sound that plays once, after a wait, from one side.
  struct Voice {
    Samples samples;
    std::size_t at = 0;
    std::size_t wait = 0;
    float gain = 1.0f;
    float pan = 0.0f;
  };

  void Call(const Samples& samples, float gain, float pan, float wait = 0.0f);

  /// Makes a recording into a loop to play in place of the bed: its end
  /// folded into its start, and as loud as the bed. False if there's
  /// nothing in it.
  bool Recorded(Bed bed, Samples& left, Samples& right) const;

  Noise noise_;
  std::array<Loop, kBeds> beds_;
  std::size_t played_ = 0;
  std::vector<Voice> voices_;
  float next_bird_ = 2.0f;
  float next_owl_ = 8.0f;
  float next_drip_ = 1.0f;
  float next_gust_ = 4.0f;
  Samples gust_;
  uint32_t takes_ = 0;
};

/// A bolt's thunder: a crack when it's near, then a long low roll.
Samples ThunderSound(float metres, uint32_t seed);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_AMBIENCE_H_
