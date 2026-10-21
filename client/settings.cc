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

absl::StatusOr<Settings> LoadSettings(const std::string& path) {
  Settings loaded;
  if (!FileExists(path.c_str())) {
    return loaded;
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
  const settings::Sound& sound = file.sound();
  if (sound.has_master() && !std::isnan(sound.master())) {
    loaded.sound.master = std::clamp(sound.master(), 0.0f, 1.0f);
  }
  if (sound.has_music() && !std::isnan(sound.music())) {
    loaded.sound.music = std::clamp(sound.music(), 0.0f, 1.0f);
  }
  if (sound.has_muted()) {
    loaded.sound.muted = sound.muted();
  }
  const settings::Graphics& graphics = file.graphics();
  if (graphics.has_bloom()) {
    loaded.graphics.bloom = graphics.bloom();
  }
  if (graphics.has_cel_shading()) {
    loaded.graphics.cel_shading = graphics.cel_shading();
  }
  return loaded;
}

absl::Status SaveSettings(const std::string& path, const Settings& player) {
  settings::Settings file;
  file.mutable_sound()->set_master(player.sound.master);
  file.mutable_sound()->set_music(player.sound.music);
  file.mutable_sound()->set_muted(player.sound.muted);
  file.mutable_graphics()->set_bloom(player.graphics.bloom);
  file.mutable_graphics()->set_cel_shading(player.graphics.cel_shading);
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
