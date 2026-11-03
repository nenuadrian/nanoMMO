#ifndef NANOMMO_CLIENT_MENU_H_
#define NANOMMO_CLIENT_MENU_H_

#include "client/atmosphere.h"
#include "client/settings.h"

namespace nanommo {

/// The game's menu, which Escape opens over the world when nothing else is
/// open, and the settings it leads to: the interface, sound, graphics, the
/// sky, and the keys.
class Menu {
 public:
  bool open() const { return open_; }
  void Toggle() { open_ = !open_; }

  bool settings_open() const { return settings_; }
  void OpenSettings() { settings_ = true; }
  void CloseSettings() { settings_ = false; }

  enum class Choice { kNothing, kLogOut, kQuit };

  /// Returns what the player picked, to log out or quit, if either. The
  /// settings window edits the player's settings, and the atmosphere, which
  /// can go back to the land's own.
  Choice Build(Settings& settings, Atmosphere& atmosphere,
               const Atmosphere& land);

  /// The row of buttons in the bottom right, for the windows the keys open,
  /// with their keys in their tooltips, and this menu's.
  void BuildButtons(bool* character, bool* bag, bool* map);

 private:
  enum class Page { kInterface, kSound, kGraphics, kSky, kKeys };

  void BuildSettings(Settings& settings, Atmosphere& atmosphere,
                     const Atmosphere& land);

  bool open_ = false;
  bool settings_ = false;
  Page page_ = Page::kInterface;
  // The interface's scale while its slider's held, which takes effect once
  // it's let go, as the slider itself would grow and shrink under the mouse.
  float scale_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MENU_H_
