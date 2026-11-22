#include "client/scene.h"

#include <array>

namespace nanommo {

namespace {

constexpr std::array<Color, kShirtColors> kPlayerColors = {{
    {242, 140, 51, 255},
    {64, 153, 242, 255},
    {89, 204, 102, 255},
    {178, 115, 242, 255},
    {242, 89, 102, 255},
    {242, 217, 77, 255},
}};

}  // namespace

Color ShirtColor(uint32_t shirt) {
  return kPlayerColors[shirt % kPlayerColors.size()];
}

Color PlayerColor(uint32_t id) { return ShirtColor(id - 1); }

}  // namespace nanommo
