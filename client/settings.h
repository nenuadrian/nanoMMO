#ifndef NANOMMO_CLIENT_SETTINGS_H_
#define NANOMMO_CLIENT_SETTINGS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "client/sound.h"

namespace nanommo {

/// Where the player's settings go: in Application Support on macOS, and in
/// the XDG config folder elsewhere, which is ~/.config unless set.
std::string SettingsPath();

/// The settings in the file, with the defaults for what it doesn't have. With
/// no file, as on a first run, they're all the defaults.
absl::StatusOr<SoundSettings> LoadSettings(const std::string& path);

/// Writes the settings next to the path, and renames them over it, as a
/// rename replaces a file in one step: a crash halfway through leaves the old
/// file whole.
absl::Status SaveSettings(const std::string& path, const SoundSettings& sound);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SETTINGS_H_
