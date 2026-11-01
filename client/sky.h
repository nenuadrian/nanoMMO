#ifndef NANOMMO_CLIENT_SKY_H_
#define NANOMMO_CLIENT_SKY_H_

#include <raylib.h>

#include "client/atmosphere.h"

namespace nanommo {

/// The sky over a land: its colour from the horizon up, and the sun.
class Sky {
 public:
  Sky();
  ~Sky();

  Sky(const Sky&) = delete;
  Sky& operator=(const Sky&) = delete;

  /// Draws the sky around the camera. Inside BeginMode3D, first.
  void Draw(const Camera3D& camera, const Atmosphere& atmosphere) const;

 private:
  Shader dome_shader_;
  Model dome_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SKY_H_
