#ifndef NANOMMO_CLIENT_PLAYER_FRAME_H_
#define NANOMMO_CLIENT_PLAYER_FRAME_H_

#include <cstdint>
#include <vector>

#include "client/animator.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Your name, level, health and experience, in the top left. Returns where
/// the frame ends, on the right, or 0 while it isn't there.
float DrawPlayerFrame(const std::vector<Figure>& figures,
                      const protocol::World& world);

/// The monster you picked, by its id, its name, level and health, beside your
/// frame, from left, while there's one.
void DrawTargetFrame(const std::vector<Figure>& figures, uint32_t target,
                     float left);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PLAYER_FRAME_H_
