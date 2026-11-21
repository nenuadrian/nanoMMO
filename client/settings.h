#ifndef NANOMMO_CLIENT_SETTINGS_H_
#define NANOMMO_CLIENT_SETTINGS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "client/graphics.h"
#include "client/sound.h"

namespace nanommo {

/// The names over heads, which show while the pointer's on someone, or they're
/// picked, and always for those the player says, with what the player says.
struct NameplateSettings {
  /// Whose show always.
  bool monsters = false;
  bool players = false;
  bool npcs = false;
  bool yours = false;
  /// What shows with a name: a health bar under it, the level before it, and
  /// a player's guild.
  bool health = true;
  bool levels = true;
  bool guilds = true;

  bool operator==(const NameplateSettings&) const = default;
};

/// How the interface looks, as the player sets it.
struct InterfaceSettings {
  /// How much bigger or smaller than its size for the screen the interface
  /// is drawn, from half to twice.
  float scale = 1.0f;
  /// Whether the minimap's in the top right, and whether it turns with the
  /// camera, so that what's ahead is up, rather than north. The map's on M
  /// either way.
  bool minimap = true;
  bool minimap_turns = false;
  /// The hotbar's skills after the swing, by their ids, in the order the
  /// player dragged them into: none, for the order of their levels.
  std::vector<uint32_t> hotbar;
  NameplateSettings nameplates;

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
