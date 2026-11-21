#ifndef NANOMMO_CLIENT_MENU_H_
#define NANOMMO_CLIENT_MENU_H_

#include "client/settings.h"

namespace nanommo {

/// The keys, and what each does, as a table, for the settings' keys and the
/// help.
void BuildKeys();

/// The game's menu, which Escape opens over the world when nothing else is
/// open, and the settings it leads to, which O opens too: the interface,
/// sound, graphics, and the keys. The sky is the server's, so there's nothing
/// of it to set, except to turn the weather, or the day and the night, off.
class Menu {
 public:
  bool open() const { return open_; }
  void Toggle() { open_ = !open_; }
  void Close() { open_ = false; }

  bool settings_open() const { return settings_; }
  void OpenSettings() { settings_ = true; }
  void CloseSettings() { settings_ = false; }

  enum class Choice { kNothing, kLogOut, kQuit };

  /// Returns what the player picked, to log out or quit, if either. The
  /// settings window edits the player's settings; reset_hotbar is set when
  /// they ask for the hotbar's own order back.
  Choice Build(Settings& settings, bool* reset_hotbar);

  /// The row of round buttons in the bottom right, for the windows the keys
  /// open, with their keys in their tooltips, and this menu's.
  void BuildButtons(bool* character, bool* bag, bool* quests, bool* guild,
                    bool* map, bool* help);

 private:
  enum class Page { kInterface, kSound, kGraphics, kKeys };

  void BuildSettings(Settings& settings, bool* reset_hotbar);

  bool open_ = false;
  bool settings_ = false;
  Page page_ = Page::kInterface;
  // The interface's scale while its slider's held, which takes effect once
  // it's let go, as the slider itself would grow and shrink under the mouse.
  float scale_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MENU_H_
