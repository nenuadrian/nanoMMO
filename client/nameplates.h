#ifndef NANOMMO_CLIENT_NAMEPLATES_H_
#define NANOMMO_CLIENT_NAMEPLATES_H_

#include <raylib.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "client/animator.h"
#include "client/settings.h"
#include "client/telegraphs.h"

namespace nanommo {

/// In metres above a figure's feet, where its nameplate goes.
float Above(const Figure& figure);

/// Whether a figure has a nameplate over it: once its name has come, as names
/// come once a second, and not while it's a dead monster, which there's
/// nothing left to fight.
bool HasNameplate(const Figure& figure);

/// What the player's taking notice of, which says whose nameplates show:
/// the one the pointer's on, by Who, the one they've picked, and those the
/// settings show always.
struct Noticed {
  NameplateSettings settings;
  std::optional<uint64_t> pointed;
  uint32_t target = 0;
  uint32_t target_player = 0;
};

/// Whether the figure's name shows: while it's pointed at or picked, and
/// otherwise as the settings say for its kind, or for you. Over an NPC with a
/// quest for you, its mark shows anyway, so that it can be found.
bool ShowsName(const Figure& figure, const Noticed& noticed);

/// Over each figure whose name shows, its name, and its level and health, but
/// for an NPC's, as the settings say. Under a monster's health, its skill on
/// its way, if it has one: a bar that fills as its shape does, in its colour,
/// and its name. The names of the players in the guild at war with yours, by
/// its name, are red.
void DrawNameplates(const std::vector<Figure>& figures, const Camera3D& camera,
                    const std::map<uint32_t, Telegraph>& telegraphs = {},
                    std::string_view enemy = {}, const Noticed& noticed = {});

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_NAMEPLATES_H_
