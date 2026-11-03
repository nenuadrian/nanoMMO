#ifndef NANOMMO_CLIENT_MENU_H_
#define NANOMMO_CLIENT_MENU_H_

#include "client/atmosphere.h"
#include "client/settings.h"
#include "client/window.h"

namespace nanommo {

class Menu {
 public:
  /// Returns true when the player picks Log out. The settings window edits
  /// the player's settings, and the atmosphere, where Reset puts back the
  /// land's own.
  bool Build(Window& window, Settings& settings, Atmosphere& atmosphere,
             const Atmosphere& land, bool* show_character, bool* show_bag);

 private:
  bool show_settings_ = false;
  bool show_demo_ = false;
  // The interface's scale while its slider's held, which takes effect once
  // it's let go, as the slider itself would grow and shrink under the mouse.
  float scale_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MENU_H_
