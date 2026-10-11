#include "bot/wander.h"

namespace nanommo {

protocol::Input RandomDirection(std::mt19937& random) {
  std::uniform_int_distribution<int> key(-1, 1);
  protocol::Input input;
  input.set_x(static_cast<float>(key(random)));
  input.set_z(static_cast<float>(key(random)));
  std::bernoulli_distribution shift(1.0 / 3.0);
  input.set_run(shift(random));
  return input;
}

std::chrono::milliseconds RandomHold(std::mt19937& random) {
  std::uniform_int_distribution<int> ms(500, 3000);
  return std::chrono::milliseconds(ms(random));
}

}  // namespace nanommo
