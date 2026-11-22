#ifndef NANOMMO_CLIENT_SCENE_H_
#define NANOMMO_CLIENT_SCENE_H_

#include <raylib.h>

#include <cstdint>

namespace nanommo {

/// How many colours there are to dye a player's clothes in.
inline constexpr int kShirtColors = 6;

/// The colour a player chose for their clothes, the shirt-th of the players'
/// colours, wrapped round to what there is.
Color ShirtColor(uint32_t shirt);

/// The colour a player's id picks for their clothes, when they didn't choose
/// one: a character from before there was a colour to choose.
Color PlayerColor(uint32_t id);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SCENE_H_
