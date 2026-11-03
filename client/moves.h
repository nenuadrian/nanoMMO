#ifndef NANOMMO_CLIENT_MOVES_H_
#define NANOMMO_CLIENT_MOVES_H_

#include <raylib.h>

#include <array>

#include "content/skills.pb.h"

namespace nanommo {

/// Each joint's bone in the rig, in the order of the joints.
inline constexpr std::array<const char*, 13> kJointBones = {
    "Hips",         "Spine",     "Spine2",  "Head",        "RightArm",
    "RightForeArm", "RightHand", "LeftArm", "LeftForeArm", "RightUpLeg",
    "RightLeg",     "LeftUpLeg", "LeftLeg"};

/// Where a model's bones are, by joint, or -1 for one it doesn't have.
using Joints = std::array<int, kJointBones.size()>;

/// A move at one moment: each joint's turn, and how far the whole body's
/// moved, in metres.
struct Bend {
  std::array<Quaternion, kJointBones.size()> turns;
  Vector3 shift{0.0f, 0.0f, 0.0f};
};

Joints FindJoints(const Model& model);

/// The move, `ms` into it, between the keys either side.
Bend BendAt(const skills::Move& move, float ms);

/// Turns the bones of a pose, as the skeleton's bones, by the bend: each joint
/// takes the bones below it round with it. A metre is this many of the
/// model's units.
void Turn(const Bend& bend, const Joints& joints, float metre,
          const ModelSkeleton& skeleton, Transform* pose);

/// Turns a model's bones, as a clip posed them, by the bend, and works out
/// the matrices the GPU skins it with.
void Apply(const Bend& bend, const Joints& joints, float metre, Model& model);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MOVES_H_
