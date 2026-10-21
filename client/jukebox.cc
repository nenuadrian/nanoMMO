#include "client/jukebox.h"

#include <algorithm>

namespace nanommo {

Jukebox::Cue Jukebox::Update(float seconds, bool in_world, bool playing) {
  if (!in_world) {
    // A player who comes back hears the track from the start, straight away.
    started_ = false;
    rest_ = 0.0f;
    if (!playing) {
      return Cue::kNothing;
    }
    volume_ = std::max(volume_ - seconds / kFadeOut, 0.0f);
    return volume_ == 0.0f ? Cue::kStop : Cue::kNothing;
  }
  if (playing) {
    volume_ = std::min(volume_ + seconds / kFadeIn, 1.0f);
    return Cue::kNothing;
  }
  // The track has played through, so it rests before it starts again.
  if (started_) {
    started_ = false;
    rest_ = kRest;
  }
  rest_ -= seconds;
  if (rest_ > 0.0f) {
    return Cue::kNothing;
  }
  started_ = true;
  volume_ = 0.0f;
  return Cue::kStart;
}

}  // namespace nanommo
