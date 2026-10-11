#ifndef NANOMMO_CLIENT_GUI_H_
#define NANOMMO_CLIENT_GUI_H_

namespace nanommo {

class Gui {
 public:
  Gui();
  ~Gui();

  Gui(const Gui&) = delete;
  Gui& operator=(const Gui&) = delete;

  void Begin();

  void End();
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GUI_H_
