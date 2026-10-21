#ifndef NANOMMO_CLIENT_INPUT_H_
#define NANOMMO_CLIENT_INPUT_H_

#include <raylib.h>

namespace nanommo {

/// Whether the player is typing into a text field, such as the chat's, which
/// takes the keys from the game.
bool Typing();

/// x is to the right and z is towards the camera, each -1, 0 or 1.
Vector3 Movement();

bool Running();

bool Attacking();

/// Whether the left button went down this frame on the world, and not on one
/// of the GUI's windows.
bool ClickedWorld();

/// How far to turn the camera this frame, in radians: with Q and E, or the
/// mouse, dragged with the right button down.
float CameraTurn(float seconds);

/// How far to tilt the camera this frame, in radians, up over the player or
/// down towards the ground: with the mouse, dragged with the right button
/// down.
float CameraTilt();

/// How much closer to bring the camera this frame, as a share of its
/// distance, from the mouse wheel.
float CameraZoom();

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_INPUT_H_
