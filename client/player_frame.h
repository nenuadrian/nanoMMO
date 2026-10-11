#ifndef NANOMMO_CLIENT_PLAYER_FRAME_H_
#define NANOMMO_CLIENT_PLAYER_FRAME_H_

#include <vector>

#include "client/animator.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

void DrawPlayerFrame(const std::vector<Figure>& figures,
                     const protocol::World& world);

}

#endif  // NANOMMO_CLIENT_PLAYER_FRAME_H_
