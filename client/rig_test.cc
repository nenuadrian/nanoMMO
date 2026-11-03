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
