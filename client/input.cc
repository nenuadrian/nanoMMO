#include "client/input.h"

namespace nanommo {

Vector3 Movement() {
  const auto down = [](int key, int arrow) {
    return IsKeyDown(key) || IsKeyDown(arrow);
  };

  Vector3 direction{0.0f, 0.0f, 0.0f};
  if (down(KEY_W, KEY_UP)) {
    direction.z -= 1.0f;
  }
  if (down(KEY_S, KEY_DOWN)) {
    direction.z += 1.0f;
  }
  if (down(KEY_A, KEY_LEFT)) {
    direction.x -= 1.0f;
  }
  if (down(KEY_D, KEY_RIGHT)) {
    direction.x += 1.0f;
  }
  return direction;
}

}  // namespace nanommo
