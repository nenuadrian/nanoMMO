#include "client/rig.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "client/moves.h"

namespace nanommo {

namespace {

constexpr float kFramesPerSecond = 60.0f;

using Pose = std::vector<Transform>;

class Poser {
 public:
  Poser(const rig::Rig& rig, const Model& model)
      : skeleton_(model.skeleton),
        rest_(model.skeleton.bindPose,
              model.skeleton.bindPose + model.skeleton.boneCount),
        joints_(FindJoints(model)) {
    skills::Move stance;
    *stance.add_keys() = rig.stance();
    standing_ = rest_;
    Turn(BendAt(stance, 0.0f), joints_, 1.0f, skeleton_, standing_.data());
  }

  // The figure standing, turned by the bend.
  Pose Bent(const Bend& bend) const {
    Pose pose = standing_;
    Turn(bend, joints_, 1.0f, skeleton_, pose.data());
    return pose;
  }

 private:
  ModelSkeleton skeleton_;
  Pose rest_;
  Pose standing_;
  Joints joints_;
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
    poses.push_back(poser.Bent(BendAt(idle, at)));
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
  // Every motion is the idle, until the legs can walk and the body can fall.
  const std::vector<Pose> idle = Idle(poser, rig.idle());
  const std::vector<ModelAnimation> clips = {
      Clip("idle", idle), Clip("walk", idle), Clip("run", idle),
      Clip("death", idle)};
  *count = static_cast<int>(clips.size());
  auto* baked = static_cast<ModelAnimation*>(MemAlloc(
      std::max<std::size_t>(clips.size(), 1) * sizeof(ModelAnimation)));
  std::ranges::copy(clips, baked);
  return baked;
}

}  // namespace nanommo
