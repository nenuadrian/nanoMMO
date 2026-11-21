#ifndef NANOMMO_CLIENT_PICKING_H_
#define NANOMMO_CLIENT_PICKING_H_

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "client/animator.h"
#include "terrain/terrain.h"

namespace nanommo {

/// The nearest NPC along the ray that the player, standing at you, is near
/// enough to talk to, learn from or take a quest from, such as the one under
/// the mouse, or nullptr if there's none.
const Figure* PickNpc(const std::vector<Figure>& figures, const Ray& ray,
                      Vector3 you);

/// Whether the figure is another player in the guild at war with yours, by
/// its name, or none for no war.
bool Enemy(const Figure& figure, std::string_view enemy);

/// The nearest living monster along the ray, or in a duel, the opponent, by
/// their id, or a player in the guild at war with yours, whom the player
/// picks as they would a monster. Or nullptr, if there's none of them.
const Figure* PickMonster(const std::vector<Figure>& figures, const Ray& ray,
                          uint32_t opponent = 0, std::string_view enemy = {});

/// The nearest figure along the ray but you, of any kind, alive or not, such
/// as the one under the mouse, whose name shows while it's there: or nullptr.
const Figure* PickAnyone(const std::vector<Figure>& figures, const Ray& ray);

/// Whether the figure is the one the player picked: the monster, by its id,
/// or the player, in a duel or at war, by theirs, as each has ids of their own.
bool Picked(const Figure& figure, uint32_t target, uint32_t target_player);

/// Where the ray first meets the ground, if it does within the farthest
/// anyone sees, such as the spot under the mouse.
std::optional<Vector3> PickGround(const Terrain& terrain, const Ray& ray);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_PICKING_H_
