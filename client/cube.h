#ifndef NANOMMO_CLIENT_CUBE_H_
#define NANOMMO_CLIENT_CUBE_H_

#include <raylib.h>

namespace nanommo {

struct Cube {
  Vector3 position{0.0f, 0.0f, 0.0f};
  Vector3 size{1.0f, 1.0f, 1.0f};
  Color color = WHITE;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CUBE_H_
