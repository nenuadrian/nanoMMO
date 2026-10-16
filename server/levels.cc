#include "server/levels.h"

namespace nanommo {

Level LevelFor(uint32_t experience) {
  Level level{.number = 1, .experience = experience, .next = 100};
  while (level.experience >= level.next) {
    level.experience -= level.next;
    ++level.number;
    level.next = 100 * level.number;
  }
  return level;
}

int MaxHealth(int level) { return 100 + 20 * (level - 1); }

int ExtraDamage(int level) { return 5 * (level - 1); }

}  // namespace nanommo
