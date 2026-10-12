#ifndef NANOMMO_CLIENT_INPUT_H_
#define NANOMMO_CLIENT_INPUT_H_

#include <raylib.h>

namespace nanommo {

/// x is to the right and z is towards the camera, each -1, 0 or 1.
Vector3 Movement();

bool Running();

bool Attacking();

/// How far to turn the camera this frame, in radians: with Q and E, or the
/// mouse, dragged with the right button down.
float CameraTurn(float seconds);

/// How much closer to bring the camera this frame, as a share of its
/// distance, from the mouse wheel.
float CameraZoom();

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_INPUT_H_
