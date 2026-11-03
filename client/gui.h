#ifndef NANOMMO_CLIENT_GUI_H_
#define NANOMMO_CLIENT_GUI_H_

namespace nanommo {

class Gui {
 public:
  Gui();
  ~Gui();

  Gui(const Gui&) = delete;
  Gui& operator=(const Gui&) = delete;

  /// Starts the frame's interface, at the scale for the screen's height times
  /// the player's own.
  void Begin(float player_scale = 1.0f);

  void End();

 private:
  float applied_ = 0.0f;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GUI_H_
