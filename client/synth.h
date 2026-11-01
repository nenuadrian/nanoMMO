#ifndef NANOMMO_CLIENT_SYNTH_H_
#define NANOMMO_CLIENT_SYNTH_H_

#include <cstdint>
#include <vector>

namespace nanommo {

/// Sound, as numbers: where the speaker's cone is, from -1 to 1, this many
/// times a second.
constexpr int kRate = 44100;

/// One channel of sound, at kRate.
using Samples = std::vector<float>;

/// Random numbers, which a seed decides, so that a sound made again from its
/// seed comes out as it did before.
class Noise {
 public:
  explicit Noise(uint32_t seed);

  /// From 0 to 1.
  float Uniform();

  /// From low to high.
  float Between(float low, float high);

  /// From -1 to 1: white noise, a sample at a time.
  float Next() { return Between(-1.0f, 1.0f); }

 private:
  uint32_t state_;
};

/// A filter that passes what's below a frequency, around it, or above it, all
/// three at once, from two running sums of what came through. The frequency
/// can change from one sample to the next, so that a sound can sweep.
class Filter {
 public:
  struct Out {
    float low;
    float band;
    float high;
  };

  /// One sample through. The higher the q, the narrower the band, and the
  /// longer it rings.
  Out Run(float in, float hertz, float q = 0.7f);

 private:
  float sum1_ = 0.0f;
  float sum2_ = 0.0f;
};

/// How loud a sound is, t seconds in: up from nothing over attack, then down
/// by about two thirds each decay seconds.
float Envelope(float t, float attack, float decay);

/// The seconds, as a count of samples.
int SamplesIn(float seconds);

/// Adds the sound into the mix, starting at that time and at that gain, making
/// the mix longer, if it needs to be.
void Mix(Samples& mix, const Samples& sound, float at, float gain = 1.0f);

/// The loudest sample, whichever way it goes.
float Peak(const Samples& samples);

/// Louder or quieter, so that the loudest sample is at the peak.
void Normalize(Samples& samples, float peak);

/// Brings the last few milliseconds down to nothing, as a sound cut off
/// mid-wave clicks.
void FadeOut(Samples& samples);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SYNTH_H_
