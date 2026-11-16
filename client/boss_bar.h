#ifndef NANOMMO_CLIENT_BOSS_BAR_H_
#define NANOMMO_CLIENT_BOSS_BAR_H_

#include <cstdint>
#include <vector>

#include "client/animator.h"
#include "content/monsters.h"

namespace nanommo {

/// In metres from you: a boss this near has its bar up, as you're in its
/// fight, or about to be. Its leash is less.
inline constexpr float kBossBarReach = 30.0f;

/// The boss whose bar is up: the monster you picked, if it's a boss, or else
/// the nearest boss within kBossBarReach, while it's alive. Or nullptr.
const Figure* ShownBoss(const std::vector<Figure>& figures, uint32_t target);

/// The boss's bar, across the top of the screen, under the target's frame:
/// its name, its health, with a mark where each phase after the first starts,
/// and the phase it's in.
void DrawBossBar(const Figure& boss, const MonsterTable& monsters);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_BOSS_BAR_H_
