#ifndef NANOMMO_CLIENT_NAMEPLATES_H_
#define NANOMMO_CLIENT_NAMEPLATES_H_

#include <raylib.h>

#include <vector>

#include "client/animator.h"

namespace nanommo {

/// In metres above a figure's feet, where its nameplate goes.
float Above(const Figure& figure);

/// Whether a figure has a nameplate over it: once its name has come, as names
/// come once a second, and not while it's a dead monster, which there's
/// nothing left to fight.
bool HasNameplate(const Figure& figure);

void DrawNameplates(const std::vector<Figure>& figures, const Camera3D& camera);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_NAMEPLATES_H_
