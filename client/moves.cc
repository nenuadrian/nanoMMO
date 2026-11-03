#include "client/moves.h"

#include <raymath.h>

#include <cstring>
#include <vector>

namespace nanommo {

namespace {

float Eased(skills::Ease ease, float t) {
  switch (ease) {
    case skills::SNAP:
      return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    case skills::GATHER:
      return t * t * t;
    default:
      return t * t * (3.0f - 2.0f * t);
  }
}

// The key's turns, by joint, in degrees: none for a joint it leaves alone.
std::array<Vector3, kJointBones.size()> Angles(const skills::Key& key) {
  std::array<Vector3, kJointBones.size()> angles{};
  for (const skills::Turn& turn : key.turns()) {
    if (turn.joint() >= 0 && turn.joint() < std::ssize(angles)) {
      angles[turn.joint()] = {turn.pitch(), turn.yaw(), turn.roll()};
    }
  }
  return angles;
}

Matrix ToMatrix(const Transform& t) {
  return MatrixMultiply(
      MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z),
                     QuaternionToMatrix(t.rotation)),
      MatrixTranslate(t.translation.x, t.translation.y, t.translation.z));
}

}  // namespace

Joints FindJoints(const Model& model) {
  Joints joints;
  joints.fill(-1);
  for (std::size_t joint = 0; joint < kJointBones.size(); ++joint) {
    for (int bone = 0; bone < static_cast<int>(model.skeleton.boneCount);
         ++bone) {
      if (std::strcmp(model.skeleton.bones[bone].name, kJointBones[joint]) ==
          0) {
        joints[joint] = bone;
      }
    }
  }
  return joints;
}

Bend BendAt(const skills::Move& move, float ms) {
  // Before the first key, it's the clip's own pose: a key with no turns.
  skills::Key from;
  skills::Key to;
  float t = 1.0f;
  for (const skills::Key& key : move.keys()) {
    to = key;
    if (ms < key.at_ms()) {
      const float span = static_cast<float>(key.at_ms() - from.at_ms());
      t = span > 0.0f ? Eased(key.ease(), (ms - from.at_ms()) / span) : 1.0f;
      break;
    }
    from = key;
  }
  const auto before = Angles(from);
  const auto after = Angles(to);
  Bend bend;
  for (std::size_t joint = 0; joint < bend.turns.size(); ++joint) {
    const Vector3 angle =
        Vector3Scale(Vector3Lerp(before[joint], after[joint], t), DEG2RAD);
    bend.turns[joint] = QuaternionFromEuler(angle.x, angle.y, angle.z);
  }
  bend.shift = {0.0f, Lerp(from.lift(), to.lift(), t),
                Lerp(from.ahead(), to.ahead(), t)};
  return bend;
}

void Apply(const Bend& bend, const Joints& joints, float metre, Model& model) {
  const int count = model.skeleton.boneCount;
  std::vector<Quaternion> turn(count, QuaternionIdentity());
  for (std::size_t joint = 0; joint < joints.size(); ++joint) {
    if (joints[joint] >= 0) {
      turn[joints[joint]] = bend.turns[joint];
    }
  }
  Transform* pose = model.currentPose;
  const std::vector<Transform> clip(pose, pose + count);
  // Each bone comes after the one it hangs from, so by the time we get to a
  // bone, its parent's in place, and turned by every joint above it.
  std::vector<Quaternion> turned(count, QuaternionIdentity());
  for (int bone = 0; bone < count; ++bone) {
    const int parent = model.skeleton.bones[bone].parent;
    if (parent < 0) {
      pose[bone].translation =
          Vector3Add(clip[bone].translation, Vector3Scale(bend.shift, metre));
      turned[bone] = turn[bone];
    } else {
      const Vector3 reach =
          Vector3Subtract(clip[bone].translation, clip[parent].translation);
      pose[bone].translation =
          Vector3Add(pose[parent].translation,
                     Vector3RotateByQuaternion(reach, turned[parent]));
      turned[bone] = QuaternionMultiply(turned[parent], turn[bone]);
    }
    pose[bone].rotation = QuaternionMultiply(turned[bone], clip[bone].rotation);
    // What the GPU skins with: from where the bone was bound to where it is.
    model.boneMatrices[bone] =
        MatrixMultiply(MatrixInvert(ToMatrix(model.skeleton.bindPose[bone])),
                       ToMatrix(pose[bone]));
  }
}

}  // namespace nanommo
