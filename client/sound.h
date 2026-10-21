#ifndef NANOMMO_CLIENT_SOUND_H_
#define NANOMMO_CLIENT_SOUND_H_

namespace nanommo {

/// How loud the game is, as the player sets it in the settings window.
struct SoundSettings {
  /// Where the sliders are, from nothing to full.
  float master = 1.0f;
  float music = 0.7f;
  bool muted = false;

  bool operator==(const SoundSettings&) const = default;
};

/// The gain for a slider at that position. We hear loudness on a logarithmic
/// scale, so the slider goes through 40 decibels, and its bottom is silence.
float Gain(float slider);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SOUND_H_
