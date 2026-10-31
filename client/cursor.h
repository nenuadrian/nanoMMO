#ifndef NANOMMO_CLIENT_CURSOR_H_
#define NANOMMO_CLIENT_CURSOR_H_

namespace nanommo {

/// What the mouse is over in the world.
enum class Pointing { kNothing, kNpc, kMonster };

/// Sets the mouse cursor for the frame: over the GUI, the GUI's own; over the
/// world, a hand on someone to talk to, and a crosshair on a monster to pick.
/// The GUI leaves the cursor to us (see Gui), so this runs each frame, once
/// the GUI is built.
void UpdateCursor(Pointing pointing);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CURSOR_H_
