#include "client/rig.h"

#include <raymath.h>

#include <string_view>
#include <vector>

#include "content/content.h"
#include "content/rig.h"
#include "gtest/gtest.h"

namespace {

nanommo::Content GamesContent() {
  return *nanommo::ReadContent(NANOMMO_CONTENT);
}

int BoneNamed(const Model& model, std::string_view name) {
  for (int i = 0; i < static_cast<int>(model.skeleton.boneCount); ++i) {
    if (name == model.skeleton.bones[i].name) {
      return i;
    }
  }
  return -1;
}

// The rig's model, with its clips, freed when it goes.
struct Baked {
  explicit Baked(const nanommo::rig::Rig& rig)
      : model(nanommo::RigModel(nanommo::Skeleton(rig, 0))),
        clips(nanommo::Bake(rig, model, &count)) {}
  ~Baked() {
    UnloadModelAnimations(clips, count);
    UnloadModel(model);
  }
  Baked(const Baked&) = delete;
  Baked& operator=(const Baked&) = delete;

  Model model;
  int count = 0;
  ModelAnimation* clips;
};

}  // namespace

TEST(Rig, MakesABoneForEachJointWhereItRests) {
  const nanommo::rig::Rig rig = GamesContent().rig;
  const Model model = nanommo::RigModel(nanommo::Skeleton(rig, 0));

  const int hips = BoneNamed(model, "Hips");
  const int arm = BoneNamed(model, "RightArm");
  const int forearm = BoneNamed(model, "RightForeArm");
  ASSERT_EQ(hips, 0);
  ASSERT_GE(forearm, 0);
  EXPECT_EQ(model.skeleton.bones[hips].parent, -1);
  EXPECT_EQ(model.skeleton.bones[forearm].parent, arm);
  // On the right, so x is below zero, and in metres.
  EXPECT_LT(model.skeleton.bindPose[forearm].translation.x, -0.3f);
  EXPECT_GT(model.skeleton.bindPose[forearm].translation.y, 1.3f);
  UnloadModel(model);
}

TEST(Rig, BakesTheIdleTheWalkTheRunAndTheDeath) {
  const Baked baked(GamesContent().rig);

  ASSERT_EQ(baked.count, 4);
  EXPECT_STREQ(baked.clips[0].name, "idle");
  EXPECT_STREQ(baked.clips[3].name, "death");
  // A loop ends where it starts.
  for (const int loop : {0, 1, 2}) {
    const ModelAnimation& clip = baked.clips[loop];
    for (unsigned bone = 0; bone < clip.boneCount; ++bone) {
      EXPECT_TRUE(Vector3Equals(
          clip.keyframePoses[0][bone].translation,
          clip.keyframePoses[clip.keyframeCount - 1][bone].translation));
    }
  }
}

TEST(Rig, KeepsAFootThatsDownWhereItWasPutDown) {
  const nanommo::rig::Rig rig = GamesContent().rig;
  const Baked baked(rig);
  const int tip = BoneNamed(baked.model, "LeftToe_End");
  const int ankle = BoneNamed(baked.model, "LeftFoot");
  for (const auto& [clip, speed] :
       {std::pair{1, rig.walk().speed()}, std::pair{2, rig.run().speed()}}) {
    const ModelAnimation& gait = baked.clips[clip];
    int down = 0;
    for (int frame = 1; frame < gait.keyframeCount; ++frame) {
      const Transform* before = gait.keyframePoses[frame - 1];
      const Transform* now = gait.keyframePoses[frame];
      // The tip of the toes on the ground in both frames goes back as fast
      // as the body goes forward, which is no speed at all on the ground.
      if (before[tip].translation.y < 0.001f &&
          now[tip].translation.y < 0.001f) {
        const float back =
            (before[tip].translation.z - now[tip].translation.z) * 60.0f;
        EXPECT_NEAR(back, speed, 0.02f) << gait.name << " frame " << frame;
        ++down;
      }
      // The ankle never goes under the ground.
      EXPECT_GT(now[ankle].translation.y, 0.05f);
    }
    // Down for a good part of the cycle.
    EXPECT_GT(down, gait.keyframeCount / 5) << gait.name;
  }
}

TEST(Rig, BendsTheLegsWithoutStretchingThem) {
  const Baked baked(GamesContent().rig);
  const int hip = BoneNamed(baked.model, "RightUpLeg");
  const int knee = BoneNamed(baked.model, "RightLeg");
  const int ankle = BoneNamed(baked.model, "RightFoot");
  const Transform* rest = baked.model.skeleton.bindPose;
  const float thigh =
      Vector3Distance(rest[hip].translation, rest[knee].translation);
  const float shin =
      Vector3Distance(rest[knee].translation, rest[ankle].translation);
  for (int clip = 0; clip < baked.count; ++clip) {
    const ModelAnimation& motion = baked.clips[clip];
    for (int frame = 0; frame < motion.keyframeCount; ++frame) {
      const Transform* pose = motion.keyframePoses[frame];
      EXPECT_NEAR(
          Vector3Distance(pose[hip].translation, pose[knee].translation), thigh,
          1e-4f);
      EXPECT_NEAR(
          Vector3Distance(pose[knee].translation, pose[ankle].translation),
          shin, 1e-4f);
      // The shin goes where the knee's turn takes it, as the GPU draws it.
      const Vector3 drawn = Vector3Add(
          pose[knee].translation,
          Vector3RotateByQuaternion(
              Vector3Subtract(rest[ankle].translation, rest[knee].translation),
              pose[knee].rotation));
      EXPECT_LT(Vector3Distance(drawn, pose[ankle].translation), 1e-3f)
          << motion.name << " frame " << frame;
    }
  }
}
