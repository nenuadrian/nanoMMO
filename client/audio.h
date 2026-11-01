#ifndef NANOMMO_CLIENT_AUDIO_H_
#define NANOMMO_CLIENT_AUDIO_H_

#include <raylib.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "client/synth.h"

namespace nanommo {

std::string MusicFolder();

/// raylib's audio device, which plays our sound through the system's. Like
/// the window, there's one, opened after the window and closed before it.
class AudioDevice {
 public:
  /// The device, or why there's none, such as in a container with no sound
  /// card.
  static absl::StatusOr<std::unique_ptr<AudioDevice>> Create();
  ~AudioDevice();

  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

 private:
  AudioDevice() = default;
};

/// A piece of music, which raylib streams from its file as it plays, a little
/// at a time, so that the whole song never sits decoded in memory.
class Track {
 public:
  /// Loading needs the device, so it takes one, even though it doesn't use it.
  static absl::StatusOr<std::unique_ptr<Track>> Load(const AudioDevice& device,
                                                     const std::string& path);
  ~Track();

  Track(const Track&) = delete;
  Track& operator=(const Track&) = delete;

  /// From the start.
  void Play();

  void Stop();

  bool playing() const;

  /// From nothing to the file's own loudness.
  void SetVolume(float volume);

  /// Refills the buffer the track plays from with what comes next in its file.
  /// raylib needs this each frame, or the buffer runs dry and the track goes
  /// quiet.
  void Update();

 private:
  explicit Track(Music music) : music_(music) {}

  Music music_;
};

/// A sound we made, ready to play, on a few voices that share its samples, so
/// that it can play again before it's over, as steps do.
class Clip {
 public:
  static std::unique_ptr<Clip> Load(const AudioDevice& device,
                                    const Samples& samples, int voices);
  ~Clip();

  Clip(const Clip&) = delete;
  Clip& operator=(const Clip&) = delete;

  /// On the voice that started longest ago: at a volume from 0 to 1, a pan
  /// from -1 for the left to 1 for the right, and a pitch, where 2 is an
  /// octave up.
  void Play(float volume, float pan, float pitch);

 private:
  explicit Clip(Sound sound) : sound_(sound) {}

  Sound sound_;
  /// raylib's aliases of the sound, which play its samples without copies.
  std::vector<Sound> voices_;
  std::size_t next_ = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_AUDIO_H_
