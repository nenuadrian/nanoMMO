#include "client/rig.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "client/moves.h"

namespace nanommo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kFramesPerSecond = 60.0f;

// The heel is this far behind the ankle: where a foot lands, and rocks
// forward from.
constexpr float kHeel = 0.04f;

// Each foot, and so each knee, points this far out from straight ahead.
constexpr float kToesOut = 6.0f * DEG2RAD;

// How much of the body's lean the head takes back, to look ahead.
constexpr float kLookUp = 0.6f;

// How far the sword hand bends back, as a share of how far its elbow bends.
constexpr float kWrist = 0.8f;

// The share of a leg's length past which it straightens more slowly than
// its foot goes.
constexpr float kStraight = 0.97f;

// Into a foot's time on the ground, as shares of it: until here it rolls off
// its heel, and from here onto its toes.
constexpr float kFlat = 0.15f;
constexpr float kRoll = 0.6f;

using Pose = std::vector<Transform>;

float Smooth(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

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

// The turns of a gait's upper body, a share of the way round its cycle, from
// when the left foot lands.
Bend Swing(const rig::Gait& gait, float phase) {
  const float wave = std::cos(2.0f * kPi * phase);
  skills::Key key;
  auto turn = [&key](skills::Joint joint, float pitch, float yaw) {
    skills::Turn& turn = *key.add_turns();
    turn.set_joint(joint);
    turn.set_pitch(pitch);
    turn.set_yaw(yaw);
  };
  // The hips turn with the leg that's forward, the left at first, and the
  // chest the other way, with the arm that's forward, and the head looks
  // ahead all the while.
  const float hips = -gait.hips_turn() * wave;
  const float chest = gait.chest_turn() * wave;
  turn(skills::HIPS, gait.lean() / 2.0f, hips);
  turn(skills::SPINE, gait.lean() / 2.0f, chest / 2.0f - hips);
  turn(skills::CHEST, 0.0f, chest / 2.0f);
  turn(skills::HEAD, -gait.lean() * kLookUp, -chest);
  // Each arm swings against its leg, and bends more as it comes forward, and
  // the sword hand bends back as its elbow does.
  const float left_forward = (1.0f - wave) / 2.0f;
  const float left_elbow = gait.elbow() * (0.7f + 0.6f * left_forward);
  const float right_elbow =
      gait.elbow() * (0.7f + 0.6f * (1.0f - left_forward));
  turn(skills::LEFT_ARM, gait.arms_back() + gait.arm_swing() * wave, 0.0f);
  turn(skills::RIGHT_ARM, gait.arms_back() - gait.arm_swing() * wave, 0.0f);
  turn(skills::LEFT_FOREARM, -left_elbow, 0.0f);
  turn(skills::RIGHT_FOREARM, -right_elbow, 0.0f);
  turn(skills::RIGHT_HAND, right_elbow * kWrist, 0.0f);
  skills::Move move;
  *move.add_keys() = key;
  Bend bend = BendAt(move, 0.0f);
  // Highest, or lowest, as each foot passes under the hips, and over the
  // side of the one that's down.
  const float middle = phase - gait.stance() / 2.0f;
  bend.shift = {gait.sway() * std::cos(2.0f * kPi * middle),
                -gait.crouch() +
                    gait.bob() * (1.0f + std::cos(4.0f * kPi * middle)) / 2.0f,
                0.0f};
  return bend;
}

// Where a foot is, a share of the way round the gait's cycle from when it
// lands. While it's down, it goes back as fast as the body goes forward, over a
// stretch whose middle is the centre's distance ahead of where it stands.
Step Footing(const Poser& poser, const Leg& leg, const rig::Gait& gait,
             float seconds, float phase, float centre) {
  const float stance = gait.stance();
  const float down = gait.speed() * stance * seconds;
  const Vector3 under = poser.Under(leg);
  const float x = leg.side * gait.width() / 2.0f;
  const float heel = gait.heel_strike() * DEG2RAD;
  const float toes = gait.toe_off() * DEG2RAD;
  auto on_ground = [&](float share) {
    float pitch = 0.0f;
    if (share < kFlat) {
      pitch = -heel * (1.0f - Smooth(share / kFlat));
    } else if (share > kRoll) {
      pitch = toes * Smooth((share - kRoll) / (1.0f - kRoll));
    }
    return poser.Place(leg, {x, 0.0f, under.z + centre + down * (0.5f - share)},
                       pitch);
  };
  if (phase < stance) {
    return on_ground(phase / stance);
  }
  // Off the ground: from where it left it to where it lands, as fast as the
  // ground at each end, lifted on the way, highest a little before halfway.
  const float t = (phase - stance) / (1.0f - stance);
  const Step from = on_ground(1.0f);
  const Step to = on_ground(0.0f);
  // Half as fast as the ground, which eases the foot off it and onto it.
  const float moving = -gait.speed() * (1.0f - stance) * seconds / 2.0f;
  const float t2 = t * t;
  const float t3 = t2 * t;
  const float z = (2 * t3 - 3 * t2 + 1) * from.ankle.z +
                  (t3 - 2 * t2 + t) * moving + (-2 * t3 + 3 * t2) * to.ankle.z +
                  (t3 - t2) * moving;
  const float lift =
      gait.lift() *
      std::sin(kPi * std::pow(t, std::log(0.5f) / std::log(0.4f)));
  return {.ankle = {Lerp(from.ankle.x, to.ankle.x, Smooth(t)),
                    Lerp(from.ankle.y, to.ankle.y, Smooth(t)) + lift, z},
          .pitch = Lerp(from.pitch, to.pitch, Smooth(t))};
}

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

// Where the middle of a foot's time down is, ahead of where it stands: so far
// that the leg reaches as far to land as it does to leave the ground, behind.
float Centre(const Poser& poser, const rig::Gait& gait, float seconds) {
  const Leg& leg = poser.legs()[0];
  const Vector3 landing = poser.Bent(Swing(gait, 0.0f))[leg.hip].translation;
  const Vector3 leaving =
      poser.Bent(Swing(gait, gait.stance()))[leg.hip].translation;
  float low = -0.5f;
  float high = 0.5f;
  for (int i = 0; i < 30; ++i) {
    const float centre = (low + high) / 2.0f;
    const float ahead = Vector3Distance(
        landing, Footing(poser, leg, gait, seconds, 0.0f, centre).ankle);
    const float behind = Vector3Distance(
        leaving,
        Footing(poser, leg, gait, seconds, gait.stance() * 0.9999f, centre)
            .ankle);
    (ahead > behind ? high : low) = centre;
  }
  return (low + high) / 2.0f;
}

std::vector<Pose> Loop(const Poser& poser, const rig::Gait& gait) {
  const int frames = Frames(static_cast<float>(gait.cycle_ms()));
  // A whole number of frames, so the cycle the feet keep up with is this.
  const float seconds = static_cast<float>(frames) / kFramesPerSecond;
  const float centre = Centre(poser, gait, seconds);
  std::vector<Pose> poses;
  for (int frame = 0; frame <= frames; ++frame) {
    const float phase = static_cast<float>(frame % frames) / frames;
    Pose pose = poser.Bent(Swing(gait, phase));
    for (const Leg& leg : poser.legs()) {
      // The right foot is half a cycle behind the left.
      const float own =
          std::fmod(phase + (leg.side > 0.0f ? 0.0f : 0.5f), 1.0f);
      poser.Reach(pose, leg, Footing(poser, leg, gait, seconds, own, centre));
    }
    poses.push_back(std::move(pose));
  }
  return poses;
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
    // The death is the idle, for now.
    clips = {Clip("idle", Idle(poser, rig.idle())),
             Clip("walk", Loop(poser, rig.walk())),
             Clip("run", Loop(poser, rig.run())),
             Clip("death", Idle(poser, rig.idle()))};
  }
  *count = static_cast<int>(clips.size());
  auto* baked = static_cast<ModelAnimation*>(MemAlloc(
      std::max<std::size_t>(clips.size(), 1) * sizeof(ModelAnimation)));
  std::ranges::copy(clips, baked);
  return baked;
}

}  // namespace nanommo
