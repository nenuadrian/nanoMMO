#ifndef NANOMMO_CLIENT_MENU_H_
#define NANOMMO_CLIENT_MENU_H_

#include <array>

#include "client/window.h"

namespace nanommo {

class Menu {
 public:
  /// Returns true when the player picks Log out.
  bool Build(Window& window, std::array<float, 3>& background);

 private:
  bool show_settings_ = false;
  bool show_demo_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MENU_H_
