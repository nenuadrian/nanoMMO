#ifndef NANOMMO_CLIENT_SCENE_H_
#define NANOMMO_CLIENT_SCENE_H_

#include <raylib.h>

#include <cstdint>
#include <vector>

#include "client/cube.h"

namespace nanommo {

std::vector<Cube> Ground();

constexpr float kMarkerHeight = 2.05f;

Color PlayerColor(uint32_t id);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SCENE_H_
