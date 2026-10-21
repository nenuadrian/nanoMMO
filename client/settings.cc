#include "client/settings.h"

#include <raylib.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "client/settings.pb.h"
#include "content/text.h"
#include "google/protobuf/text_format.h"

namespace nanommo {

namespace {

std::string Environment(const char* name, const std::string& otherwise) {
  const char* value = std::getenv(name);
  return value != nullptr && *value != '\0' ? value : otherwise;
}

}  // namespace

std::string SettingsPath() {
  const std::string home = Environment("HOME", ".");
#if defined(__APPLE__)
  return home + "/Library/Application Support/nanoMMO/settings.txtpb";
#else
  return Environment("XDG_CONFIG_HOME", home + "/.config") +
         "/nanommo/settings.txtpb";
#endif
}

absl::StatusOr<SoundSettings> LoadSettings(const std::string& path) {
  SoundSettings sound;
  if (!FileExists(path.c_str())) {
    return sound;
  }
  const absl::StatusOr<std::string> text = ReadFile(path);
  if (!text.ok()) {
    return text.status();
  }
  settings::Settings file;
  const absl::Status parsed = ParseText(*text, file);
  if (!parsed.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat(path, ", ", parsed.message()));
  }
  // A player can write anything into the file, and a volume past full would
  // play many times louder, so each stays between nothing and full. The text
  // format reads nan too, which std::clamp lets through, so a volume that isn't
  // a number keeps its default.
  if (file.sound().has_master() && !std::isnan(file.sound().master())) {
    sound.master = std::clamp(file.sound().master(), 0.0f, 1.0f);
  }
  if (file.sound().has_music() && !std::isnan(file.sound().music())) {
    sound.music = std::clamp(file.sound().music(), 0.0f, 1.0f);
  }
  if (file.sound().has_muted()) {
    sound.muted = file.sound().muted();
  }
  return sound;
}

absl::Status SaveSettings(const std::string& path, const SoundSettings& sound) {
  settings::Settings file;
  file.mutable_sound()->set_master(sound.master);
  file.mutable_sound()->set_music(sound.music);
  file.mutable_sound()->set_muted(sound.muted);
  std::string text;
  if (!google::protobuf::TextFormat::PrintToString(file, &text)) {
    return absl::InternalError("couldn't print the settings");
  }

  // raylib makes the folders on the way too.
  MakeDirectory(GetDirectoryPath(path.c_str()));
  const std::string written = path + ".new";
  {
    std::ofstream out(written, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out.flush()) {
      return absl::UnavailableError(absl::StrCat("couldn't write ", written));
    }
  }
  if (std::rename(written.c_str(), path.c_str()) != 0) {
    return absl::UnavailableError(
        absl::StrCat("couldn't replace ", path, ": ", std::strerror(errno)));
  }
  return absl::OkStatus();
}

}  // namespace nanommo
