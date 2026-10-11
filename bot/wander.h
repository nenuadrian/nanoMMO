#ifndef NANOMMO_BOT_WANDER_H_
#define NANOMMO_BOT_WANDER_H_

#include <chrono>
#include <random>

#include "protocol/nanommo.pb.h"

namespace nanommo {

protocol::Input RandomDirection(std::mt19937& random);

std::chrono::milliseconds RandomHold(std::mt19937& random);

}  // namespace nanommo

#endif  // NANOMMO_BOT_WANDER_H_
