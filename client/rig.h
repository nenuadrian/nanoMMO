#ifndef NANOMMO_CLIENT_RIG_H_
#define NANOMMO_CLIENT_RIG_H_

#include <raylib.h>

#include <vector>

#include "content/rig.pb.h"

namespace nanommo {

/// A model of the joints, with no mesh yet: a bone for each joint, parent
/// first, resting where the joint is, in metres, unturned. Dress gives it its
/// body.
Model RigModel(const std::vector<rig::Joint>& joints);

/// The rig's motions, worked out for a model of it, as raylib's clips: the
/// idle, the walk, the run and the death, in the protocol's order of motions,
/// sixty frames a second, each the idle for now. A loop's last frame is its
/// first again. UnloadModelAnimations frees them.
ModelAnimation* Bake(const rig::Rig& rig, const Model& model, int* count);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RIG_H_
