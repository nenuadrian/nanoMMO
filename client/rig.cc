#include "client/rig.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "client/moves.h"

namespace nanommo {

namespace {

constexpr float kFramesPerSecond = 60.0f;

// The heel is this far behind the ankle: where a foot lands, and rocks
// forward from.
constexpr float kHeel = 0.04f;

// Each foot, and so each knee, points this far out from straight ahead.
constexpr float kToesOut = 6.0f * DEG2RAD;

// The share of a leg's length past which it straightens more slowly than
// its foot goes.
constexpr float kStraight = 0.97f;

using Pose = std::vector<Transform>;

int BoneNamed(const ModelSkeleton& skeleton, std::string_view name) {
  for (int i = 0; i < static_cast<int>(skeleton.boneCount); ++i) {
    if (name == skeleton.bones[i].name) {
      return i;
    }
  }
  return -1;
}

// A turn that takes one frame to another, each given by the way a bone goes
// and the way its front faces, square to it.
Quaternion Aim(Vector3 from, Vector3 from_front, Vector3 to, Vector3 to_front) {
  // The turn from the rig's own axes to the frame: y along the bone, z to its
  // front.
  auto frame = [](Vector3 along, Vector3 front) {
    const Vector3 y = Vector3Normalize(along);
    const Vector3 z = Vector3Normalize(
        Vector3Subtract(front, Vector3Scale(y, Vector3DotProduct(front, y))));
    const Vector3 x = Vector3CrossProduct(y, z);
    return QuaternionFromMatrix(Matrix{x.x, y.x, z.x, 0.0f, x.y, y.y, z.y, 0.0f,
                                       x.z, y.z, z.z, 0.0f, 0.0f, 0.0f, 0.0f,
                                       1.0f});
  };
  return QuaternionNormalize(QuaternionMultiply(
      frame(to, to_front), QuaternionInvert(frame(from, from_front))));
}

// A leg, by its bones, and which side it's on: one to the left, minus one to
// the right.
struct Leg {
  int hip = -1;
  int knee = -1;
  int ankle = -1;
  int ball = -1;
  int tip = -1;
  float side = 1.0f;
};

// Where a foot is: its ankle, and how far it tips, toes down.
struct Step {
  Vector3 ankle;
  float pitch = 0.0f;
};

class Poser {
 public:
  Poser(const rig::Rig& rig, const Model& model)
      : skeleton_(model.skeleton),
        rest_(model.skeleton.bindPose,
              model.skeleton.bindPose + model.skeleton.boneCount),
        joints_(FindJoints(model)) {
    for (const float side : {1.0f, -1.0f}) {
      const std::string prefix = side > 0.0f ? "Left" : "Right";
      legs_.push_back({.hip = Bone(prefix + "UpLeg"),
                       .knee = Bone(prefix + "Leg"),
                       .ankle = Bone(prefix + "Foot"),
                       .ball = Bone(prefix + "ToeBase"),
                       .tip = Bone(prefix + "Toe_End"),
                       .side = side});
    }
    skills::Move stance;
    *stance.add_keys() = rig.stance();
    standing_ = rest_;
    Turn(BendAt(stance, 0.0f), joints_, 1.0f, skeleton_, standing_.data());
  }

  bool HasLegs() const {
    return std::ranges::all_of(legs_, [](const Leg& leg) {
      return leg.hip >= 0 && leg.knee >= 0 && leg.ankle >= 0;
    });
  }

  const std::vector<Leg>& legs() const { return legs_; }

  // The figure standing, turned by the bend.
  Pose Bent(const Bend& bend) const {
    Pose pose = standing_;
    Turn(bend, joints_, 1.0f, skeleton_, pose.data());
    return pose;
  }

  // Where the foot is when it stands: under its ankle in the rig.
  Vector3 Under(const Leg& leg) const {
    const Vector3 ankle = rest_[leg.ankle].translation;
    return {ankle.x, 0.0f, ankle.z};
  }

  // Where the ankle is, for a foot whose point under its ankle, as it would
  // be flat, is on the ground here, tipped so far onto its toes, or back onto
  // its heel, which stays where it touches the ground.
  Step Place(const Leg& leg, Vector3 ground, float pitch) const {
    const Vector3 rest_ankle = rest_[leg.ankle].translation;
    const Vector3 under = {rest_ankle.x, 0.0f, rest_ankle.z};
    const Vector3 end = pitch > 0.0f && leg.tip >= 0
                            ? Vector3{rest_[leg.tip].translation.x, 0.0f,
                                      rest_[leg.tip].translation.z}
                            : Vector3{under.x, 0.0f, under.z - kHeel};
    const Vector3 touch = Vector3Add(
        ground,
        Vector3RotateByQuaternion(Vector3Subtract(end, under), Yaw(leg)));
    return {.ankle = Vector3Add(
                touch, Vector3RotateByQuaternion(
                           Vector3Subtract(rest_ankle, end), Foot(leg, pitch))),
            .pitch = pitch};
  }

  // The leg bent to put its ankle where the step says, with the knee over the
  // foot.
  void Reach(Pose& pose, const Leg& leg, const Step& step) const {
    const Vector3 rest_ankle = rest_[leg.ankle].translation;
    const Quaternion yaw = Yaw(leg);
    const Quaternion foot = Foot(leg, step.pitch);
    const Vector3 ankle = step.ankle;
    const Vector3 hip = pose[leg.hip].translation;
    const Vector3 rest_hip = rest_[leg.hip].translation;
    const Vector3 rest_knee = rest_[leg.knee].translation;
    const float thigh = Vector3Distance(rest_hip, rest_knee);
    const float shin = Vector3Distance(rest_knee, rest_ankle);
    const Vector3 front = Vector3RotateByQuaternion({0.0f, 0.0f, 1.0f}, yaw);
    // The knee bends forward, in the plane of the hip, the ankle and the way
    // the foot points, as far as the two bones' lengths say.
    const Vector3 gap = Vector3Subtract(ankle, hip);
    // Past most of the leg's length, the foot falls a little short, more so
    // the further it is, so the knee straightens smoothly rather than snap.
    const float length = thigh + shin;
    const float soft = kStraight * length;
    float reach = std::max(Vector3Length(gap), std::fabs(thigh - shin));
    if (reach > soft) {
      const float rest = length - soft;
      reach = soft + rest * (1.0f - std::exp(-(reach - soft) / rest));
    }
    const Vector3 along = Vector3Normalize(gap);
    const Vector3 out = Vector3Normalize(Vector3Subtract(
        front, Vector3Scale(along, Vector3DotProduct(front, along))));
    const float cosine =
        (thigh * thigh + reach * reach - shin * shin) / (2.0f * thigh * reach);
    const float sine = std::sqrt(std::max(0.0f, 1.0f - cosine * cosine));
    const Vector3 knee =
        Vector3Add(hip, Vector3Add(Vector3Scale(along, thigh * cosine),
                                   Vector3Scale(out, thigh * sine)));
    const Vector3 placed = Vector3Add(hip, Vector3Scale(along, reach));

    const Vector3 ahead{0.0f, 0.0f, 1.0f};
    pose[leg.hip].rotation = Aim(Vector3Subtract(rest_knee, rest_hip), ahead,
                                 Vector3Subtract(knee, hip), front);
    pose[leg.knee] = {
        .translation = knee,
        .rotation = Aim(Vector3Subtract(rest_ankle, rest_knee), ahead,
                        Vector3Subtract(placed, knee), front),
        .scale = {1.0f, 1.0f, 1.0f}};
    pose[leg.ankle] = {
        .translation = placed, .rotation = foot, .scale = {1.0f, 1.0f, 1.0f}};
    // The foot is one piece, toes and all.
    if (leg.ball >= 0) {
      pose[leg.ball] = {
          .translation = Vector3Add(
              placed,
              Vector3RotateByQuaternion(
                  Vector3Subtract(rest_[leg.ball].translation, rest_ankle),
                  foot)),
          .rotation = foot,
          .scale = {1.0f, 1.0f, 1.0f}};
    }
    if (leg.ball >= 0 && leg.tip >= 0) {
      pose[leg.tip] = {.translation = Vector3Add(
                           pose[leg.ball].translation,
                           Vector3RotateByQuaternion(
                               Vector3Subtract(rest_[leg.tip].translation,
                                               rest_[leg.ball].translation),
                               foot)),
                       .rotation = foot,
                       .scale = {1.0f, 1.0f, 1.0f}};
    }
  }

 private:
  int Bone(const std::string& name) const { return BoneNamed(skeleton_, name); }

  static Quaternion Yaw(const Leg& leg) {
    return QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f}, leg.side * kToesOut);
  }

  static Quaternion Foot(const Leg& leg, float pitch) {
    return QuaternionMultiply(
        Yaw(leg), QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, pitch));
  }

  ModelSkeleton skeleton_;
  Pose rest_;
  Pose standing_;
  Joints joints_;
  std::vector<Leg> legs_;
};

ModelAnimation Clip(const char* name, const std::vector<Pose>& frames) {
  ModelAnimation clip{};
  std::strncpy(clip.name, name, sizeof(clip.name) - 1);
  clip.boneCount = frames.empty() ? 0 : frames[0].size();
  clip.keyframeCount = static_cast<int>(frames.size());
  clip.keyframePoses =
      static_cast<Transform**>(MemAlloc(frames.size() * sizeof(Transform*)));
  for (std::size_t i = 0; i < frames.size(); ++i) {
    clip.keyframePoses[i] =
        static_cast<Transform*>(MemAlloc(frames[i].size() * sizeof(Transform)));
    std::ranges::copy(frames[i], clip.keyframePoses[i]);
  }
  return clip;
}

int Frames(float ms) {
  return std::max(
      1, static_cast<int>(std::lround(ms * kFramesPerSecond / 1000.0f)));
}

std::vector<Pose> Idle(const Poser& poser, const skills::Move& idle) {
  const float ms = idle.keys().empty()
                       ? 1000.0f
                       : static_cast<float>(idle.keys().rbegin()->at_ms());
  const int frames = Frames(ms);
  std::vector<Pose> poses;
  for (int frame = 0; frame <= frames; ++frame) {
    const float at = ms * static_cast<float>(frame % frames) / frames;
    Pose pose = poser.Bent(BendAt(idle, at));
    // The feet stay where they stand, and the knees give.
    for (const Leg& leg : poser.legs()) {
      poser.Reach(pose, leg, poser.Place(leg, poser.Under(leg), 0.0f));
    }
    poses.push_back(std::move(pose));
  }
  return poses;
}

}  // namespace

Model RigModel(const std::vector<rig::Joint>& joints) {
  Model model{};
  model.transform = MatrixIdentity();
  model.materialCount = 1;
  model.materials = static_cast<Material*>(MemAlloc(sizeof(Material)));
  model.materials[0] = LoadMaterialDefault();
  const int count = static_cast<int>(joints.size());
  model.skeleton.boneCount = count;
  model.skeleton.bones =
      static_cast<BoneInfo*>(MemAlloc(count * sizeof(BoneInfo)));
  model.skeleton.bindPose =
      static_cast<Transform*>(MemAlloc(count * sizeof(Transform)));
  model.currentPose =
      static_cast<Transform*>(MemAlloc(count * sizeof(Transform)));
  model.boneMatrices = static_cast<Matrix*>(MemAlloc(count * sizeof(Matrix)));
  for (int i = 0; i < count; ++i) {
    const rig::Joint& joint = joints[i];
    BoneInfo& bone = model.skeleton.bones[i];
    std::strncpy(bone.name, joint.name().c_str(), sizeof(bone.name) - 1);
    bone.parent = -1;
    for (int j = 0; j < i; ++j) {
      if (joints[j].name() == joint.parent()) {
        bone.parent = j;
      }
    }
    model.skeleton.bindPose[i] = {
        .translation = {joint.at().x(), joint.at().y(), joint.at().z()},
        .rotation = QuaternionIdentity(),
        .scale = {1.0f, 1.0f, 1.0f}};
    model.currentPose[i] = model.skeleton.bindPose[i];
    model.boneMatrices[i] = MatrixIdentity();
  }
  return model;
}

ModelAnimation* Bake(const rig::Rig& rig, const Model& model, int* count) {
  const Poser poser(rig, model);
  std::vector<ModelAnimation> clips;
  if (poser.HasLegs()) {
    // The walk, the run and the death are the idle, for now.
    clips = {Clip("idle", Idle(poser, rig.idle())),
             Clip("walk", Idle(poser, rig.idle())),
             Clip("run", Idle(poser, rig.idle())),
             Clip("death", Idle(poser, rig.idle()))};
  }
  *count = static_cast<int>(clips.size());
  auto* baked = static_cast<ModelAnimation*>(MemAlloc(
      std::max<std::size_t>(clips.size(), 1) * sizeof(ModelAnimation)));
  std::ranges::copy(clips, baked);
  return baked;
}

}  // namespace nanommo
