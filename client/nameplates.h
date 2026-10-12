#ifndef NANOMMO_CLIENT_NAMEPLATES_H_
#define NANOMMO_CLIENT_NAMEPLATES_H_

#include <raylib.h>

#include <vector>

#include "client/animator.h"

namespace nanommo {

void DrawNameplates(const std::vector<Figure>& figures, const Camera3D& camera);

}

#endif  // NANOMMO_CLIENT_NAMEPLATES_H_
