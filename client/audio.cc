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
  return absl::WrapUnique(new Track(music));
}

Track::~Track() { UnloadMusicStream(music_); }

void Track::Play() { PlayMusicStream(music_); }

void Track::Stop() { StopMusicStream(music_); }

bool Track::playing() const { return IsMusicStreamPlaying(music_); }

void Track::Update() { UpdateMusicStream(music_); }

}  // namespace nanommo
