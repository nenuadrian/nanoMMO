#ifndef NANOMMO_CLIENT_PICKING_H_
#define NANOMMO_CLIENT_PICKING_H_

#include <raylib.h>

#include <vector>

#include "client/animator.h"

namespace nanommo {

/// The nearest NPC along the ray that the player can talk to or learn from,
/// such as the one under the mouse, or nullptr if there's none.
const Figure* PickNpc(const std::vector<Figure>& figures, const Ray& ray);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PICKING_H_
