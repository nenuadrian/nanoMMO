#ifndef NANOMMO_CLIENT_NAMEPLATES_H_
#define NANOMMO_CLIENT_NAMEPLATES_H_

#include <raylib.h>

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

#include "client/animator.h"
#include "client/telegraphs.h"

namespace nanommo {

/// In metres above a figure's feet, where its nameplate goes.
float Above(const Figure& figure);

/// Whether a figure has a nameplate over it: once its name has come, as names
/// come once a second, and not while it's a dead monster, which there's
/// nothing left to fight.
bool HasNameplate(const Figure& figure);

/// Over each figure, its name, and its level and health, but for an NPC's.
/// Under a monster's health, its skill on its way, if it has one: a bar that
/// fills as its shape does, in its colour, and its name. The names of the
/// players in the guild at war with yours, by its name, are red.
void DrawNameplates(const std::vector<Figure>& figures, const Camera3D& camera,
                    const std::map<uint32_t, Telegraph>& telegraphs = {},
                    std::string_view enemy = {});

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_NAMEPLATES_H_
