#ifndef NANOMMO_CLIENT_SCENE_H_
#define NANOMMO_CLIENT_SCENE_H_

#include <vector>

#include "client/cube.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

std::vector<Cube> Ground();

std::vector<Cube> Scene(const protocol::World& world);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SCENE_H_
