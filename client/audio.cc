#include "client/audio.h"

#include <raylib.h>

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

std::string MusicFolder() {
  return std::string(GetApplicationDirectory()) + "music";
}

absl::StatusOr<std::unique_ptr<AudioDevice>> AudioDevice::Create() {
  InitAudioDevice();
  if (!IsAudioDeviceReady()) {
    return absl::UnavailableError("couldn't open the audio device");
  }
  return absl::WrapUnique(new AudioDevice());
}

AudioDevice::~AudioDevice() { CloseAudioDevice(); }

absl::StatusOr<std::unique_ptr<Track>> Track::Load(
    const AudioDevice& /*device*/, const std::string& path) {
  Music music = LoadMusicStream(path.c_str());
  if (!IsMusicValid(music)) {
    return absl::NotFoundError(absl::StrCat("couldn't load ", path));
  }
  // raylib loops music by default, and the jukebox decides when it plays
  // again.
  music.looping = false;
  return absl::WrapUnique(new Track(music));
}

Track::~Track() { UnloadMusicStream(music_); }

void Track::Play() { PlayMusicStream(music_); }

void Track::Stop() { StopMusicStream(music_); }

bool Track::playing() const { return IsMusicStreamPlaying(music_); }

void Track::SetVolume(float volume) { SetMusicVolume(music_, volume); }

void Track::Update() { UpdateMusicStream(music_); }

std::unique_ptr<Clip> Clip::Load(const AudioDevice& /*device*/,
                                 const Samples& samples, int voices) {
  // raylib copies the samples into its own format as it loads them.
  Wave wave = {
      .frameCount = static_cast<unsigned int>(samples.size()),
      .sampleRate = kRate,
      .sampleSize = 32,
      .channels = 1,
      .data = const_cast<float*>(samples.data()),
  };
  auto clip = absl::WrapUnique(new Clip(LoadSoundFromWave(wave)));
  for (int i = 0; i < voices; ++i) {
    clip->voices_.push_back(LoadSoundAlias(clip->sound_));
  }
  return clip;
}

Clip::~Clip() {
  for (const Sound& voice : voices_) {
    UnloadSoundAlias(voice);
  }
  UnloadSound(sound_);
}

void Clip::Play(float volume, float pan, float pitch) {
  if (voices_.empty()) {
    return;
  }
  const Sound& voice = voices_[next_];
  next_ = (next_ + 1) % voices_.size();
  SetSoundVolume(voice, volume);
  SetSoundPan(voice, pan);
  SetSoundPitch(voice, pitch);
  PlaySound(voice);
}

}  // namespace nanommo
