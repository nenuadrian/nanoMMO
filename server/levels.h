#ifndef NANOMMO_SERVER_LEVELS_H_
#define NANOMMO_SERVER_LEVELS_H_

#include <cstdint>

namespace nanommo {

struct Level {
  int number = 1;
  uint32_t experience = 0;
  uint32_t next = 0;
};

Level LevelFor(uint32_t experience);

int MaxHealth(int level);

/// What a level adds to the damage of the weapon a player holds.
int ExtraDamage(int level);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LEVELS_H_
