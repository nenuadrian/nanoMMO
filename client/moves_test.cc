#include "client/moves.h"

#include <raylib.h>
#include <raymath.h>

#include <array>

#include "gtest/gtest.h"

namespace {

// A key that bows the chest forward by the pitch, in degrees.
nanommo::skills::Key Bow(uint32_t at_ms, float pitch,
                         nanommo::skills::Ease ease = nanommo::skills::SMOOTH) {
  nanommo::skills::Key key;
  key.set_at_ms(at_ms);
  key.set_ease(ease);
  nanommo::skills::Turn* turn = key.add_turns();
  turn->set_joint(nanommo::skills::CHEST);
  turn->set_pitch(pitch);
  return key;
}

// How far the bend bows the chest, in degrees.
float Pitch(const nanommo::Bend& bend) {
  return QuaternionToEuler(bend.turns[nanommo::skills::CHEST]).x * RAD2DEG;
}

}  // namespace

TEST(Moves, StartFromTheClipAndHoldTheLastKey) {
  nanommo::skills::Move move;
  *move.add_keys() = Bow(200, 40.0f);

  EXPECT_NEAR(Pitch(nanommo::BendAt(move, 0.0f)), 0.0f, 0.01f);
  // Smooth is half way there, half way through.
  EXPECT_NEAR(Pitch(nanommo::BendAt(move, 100.0f)), 20.0f, 0.01f);
  EXPECT_NEAR(Pitch(nanommo::BendAt(move, 900.0f)), 40.0f, 0.01f);
}

TEST(Moves, SnapGetsThereSoonerThanSmooth) {
  nanommo::skills::Move smooth;
  *smooth.add_keys() = Bow(100, 60.0f);
  nanommo::skills::Move snap;
  *snap.add_keys() = Bow(100, 60.0f, nanommo::skills::SNAP);

  EXPECT_GT(Pitch(nanommo::BendAt(snap, 25.0f)),
            Pitch(nanommo::BendAt(smooth, 25.0f)));
}

TEST(Moves, TurnTheBonesBelowAJoint) {
  // Three bones a metre apart, up the y axis: the hips, the chest, the head.
  std::array<BoneInfo, 3> bones{};
  std::array<Transform, 3> bind{};
  std::array<Transform, 3> pose{};
  std::array<Matrix, 3> skin{};
  for (int i = 0; i < 3; ++i) {
    bones[i].parent = i - 1;
    bind[i] = {.translation = {0.0f, static_cast<float>(i), 0.0f},
               .rotation = QuaternionIdentity(),
               .scale = {1.0f, 1.0f, 1.0f}};
    pose[i] = bind[i];
  }
  Model model{};
  model.skeleton.boneCount = 3;
  model.skeleton.bones = bones.data();
  model.skeleton.bindPose = bind.data();
  model.currentPose = pose.data();
  model.boneMatrices = skin.data();
  nanommo::Joints joints;
  joints.fill(-1);
  joints[nanommo::skills::CHEST] = 1;
  nanommo::skills::Move move;
  *move.add_keys() = Bow(100, 90.0f);

  nanommo::Apply(nanommo::BendAt(move, 100.0f), joints, 1.0f, model);

  // The chest stays where it is, and the head bows forward with it, along z.
  const Vector3 head = pose[2].translation;
  EXPECT_NEAR(head.x, 0.0f, 1e-5f);
  EXPECT_NEAR(head.y, 1.0f, 1e-5f);
  EXPECT_NEAR(head.z, 1.0f, 1e-5f);
  // And the GPU takes what was bound at the head there too.
  const Vector3 skinned = Vector3Transform({0.0f, 2.0f, 0.0f}, skin[2]);
  EXPECT_NEAR(skinned.y, 1.0f, 1e-5f);
  EXPECT_NEAR(skinned.z, 1.0f, 1e-5f);
}
