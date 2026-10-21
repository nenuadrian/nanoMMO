#ifndef NANOMMO_CLIENT_JUKEBOX_H_
#define NANOMMO_CLIENT_JUKEBOX_H_

namespace nanommo {

/// When the music plays, and how loud, kept apart from raylib so that it can
/// be tested without a sound card. The music fades in as the player enters the
/// world, plays the track through, rests for a while in silence and plays it
/// again, and it fades out as they leave.
class Jukebox {
 public:
  /// In seconds.
  static constexpr float kFadeIn = 3.0f;
  static constexpr float kFadeOut = 1.5f;
  static constexpr float kRest = 45.0f;

  /// What the track should do this frame.
  enum class Cue { kNothing, kStart, kStop };

  /// Moves on by the frame's time, from whether the player is in the world,
  /// and whether the track is playing.
  Cue Update(float seconds, bool in_world, bool playing);

  /// From nothing to the track's full volume.
  float volume() const { return volume_; }

 private:
  float volume_ = 0.0f;
  float rest_ = 0.0f;
  bool started_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_JUKEBOX_H_
