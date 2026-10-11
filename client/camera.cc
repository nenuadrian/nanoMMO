#include "client/camera.h"

namespace nanommo {

Camera3D GameCamera() {
  return {.position = {0.0f, 11.0f, 10.0f},
          .target = {0.0f, 0.0f, 0.0f},
          .up = {0.0f, 1.0f, 0.0f},
          .fovy = 50.0f,
          .projection = CAMERA_PERSPECTIVE};
}

}  // namespace nanommo
