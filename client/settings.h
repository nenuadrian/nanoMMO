#ifndef NANOMMO_CLIENT_SETTINGS_H_
#define NANOMMO_CLIENT_SETTINGS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "client/graphics.h"
#include "client/sound.h"

namespace nanommo {

/// How the interface looks, as the player sets it.
struct InterfaceSettings {
  /// How much bigger or smaller than its size for the screen the interface
  /// is drawn, from half to twice.
  float scale = 1.0f;
  /// Whether the minimap turns with the camera, so that what's ahead is up,
  /// rather than north.
  bool minimap_turns = false;

  bool operator==(const InterfaceSettings&) const = default;
};

/// What the player sets, which the client keeps between runs.
struct Settings {
  SoundSettings sound;
  GraphicsSettings graphics;
  InterfaceSettings ui;

  bool operator==(const Settings&) const = default;
};

/// Where the player's settings go: in Application Support on macOS, and in
/// the XDG config folder elsewhere, which is ~/.config unless set.
std::string SettingsPath();

/// The settings in the file, with the defaults for what it doesn't have. With
/// no file, as on a first run, they're all the defaults.
absl::StatusOr<Settings> LoadSettings(const std::string& path);

/// Writes the settings next to the path, and renames them over it, as a
/// rename replaces a file in one step: a crash halfway through leaves the old
/// file whole.
absl::Status SaveSettings(const std::string& path, const Settings& player);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SETTINGS_H_
