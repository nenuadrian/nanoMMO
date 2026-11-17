#include "client/audio.h"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
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

namespace {

// Samples a channel in each buffer: about a twentieth of a second.
constexpr int kStreamBuffer = 2048;

}  // namespace

std::unique_ptr<Stream> Stream::Open(const AudioDevice& /*device*/) {
  SetAudioStreamBufferSizeDefault(kStreamBuffer);
  auto stream = absl::WrapUnique(new Stream(LoadAudioStream(kRate, 32, 2)));
  stream->left_.resize(kStreamBuffer);
  stream->right_.resize(kStreamBuffer);
  stream->both_.resize(2 * kStreamBuffer);
  PlayAudioStream(stream->stream_);
  return stream;
}

Stream::~Stream() { UnloadAudioStream(stream_); }

void Stream::Feed(
    const std::function<void(std::span<float>, std::span<float>)>& fill) {
  while (IsAudioStreamProcessed(stream_)) {
    std::fill(left_.begin(), left_.end(), 0.0f);
    std::fill(right_.begin(), right_.end(), 0.0f);
    fill(left_, right_);
    // raylib takes the channels in turn, a sample of each.
    for (std::size_t i = 0; i < left_.size(); ++i) {
      both_[2 * i] = left_[i];
      both_[2 * i + 1] = right_[i];
    }
    UpdateAudioStream(stream_, both_.data(), kStreamBuffer);
  }
}

void Stream::SetVolume(float volume) { SetAudioStreamVolume(stream_, volume); }

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
