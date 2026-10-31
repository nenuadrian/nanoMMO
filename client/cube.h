#ifndef NANOMMO_CLIENT_CUBE_H_
#define NANOMMO_CLIENT_CUBE_H_

#include <raylib.h>

#include "client/triangles.h"

namespace nanommo {

/// What a cube is drawn as, at its size: a box, a round slab, or a standing
/// stone that comes to a point.
enum class Shape { kBox, kDisc, kSpire };

struct Cube {
  Vector3 position{0.0f, 0.0f, 0.0f};
  Vector3 size{1.0f, 1.0f, 1.0f};
  Color color = WHITE;
  Shape shape = Shape::kBox;
};

/// A disc a unit across and a unit tall, around its middle, with flat sides.
Triangles Disc();

/// A square stone a unit across and a unit tall, around its middle, a little
/// narrower towards the top, where it comes to a point.
Triangles Spire();

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CUBE_H_
