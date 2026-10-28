#include "client/figures.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "client/characters.h"
#include "client/window.h"
#include "content/figures.h"
#include "gtest/gtest.h"

namespace {

int BoneNamed(const Model& model, const std::string& name) {
  for (int i = 0; i < model.skeleton.boneCount; ++i) {
    if (name == model.skeleton.bones[i].name) {
      return i;
    }
  }
  return -1;
}

}  // namespace

TEST(Figures, BindsEachCornerToItsPartsBone) {
  const std::string path = nanommo::CharactersFolder() + "/x_bot.glb";
  if (!FileExists(path.c_str())) {
    GTEST_SKIP() << "no characters: see assets/mixamo/README.md";
  }
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  Model model = LoadModel(path.c_str());
  // An arm, a head, and a tail, which Mixamo's rig doesn't have.
  const nanommo::figures::Figures figures = *nanommo::ParseFigures(R"pb(
    body {
      shape: SHAPE_LIMB
      bone: "LeftArm"
      to: "LeftForeArm"
      start_radius: 0.06
      end_radius: 0.05
    }
    body {
      shape: SHAPE_BALL
      bone: "Head"
      size { x: 0.2 y: 0.2 z: 0.2 }
    }
    body {
      shape: SHAPE_BALL
      bone: "Tail"
      size { x: 0.2 y: 0.2 z: 0.2 }
    }
  )pb");

  nanommo::Dress(model, nanommo::Unfold(figures.body()), 1.0f);

  // A limb is eight sides of six rings, which is 64 triangles, and a ball is
  // an icosahedron's 20 faces split in four, and there's no tail.
  ASSERT_EQ(model.meshCount, 1);
  const Mesh& mesh = model.meshes[0];
  ASSERT_EQ(mesh.triangleCount, 64 + 80);
  const int arm = BoneNamed(model, "mixamorig:LeftArm");
  const int head = BoneNamed(model, "mixamorig:Head");
  for (int i = 0; i < mesh.vertexCount; ++i) {
    EXPECT_EQ(mesh.boneIndices[4 * i], i < 64 * 3 ? arm : head);
    EXPECT_FLOAT_EQ(mesh.boneWeights[4 * i], 1.0f);
    EXPECT_FLOAT_EQ(mesh.boneWeights[4 * i + 1], 0.0f);
  }
  UnloadModel(model);
  // raylib 6.0's UnloadModel leaves a model's pose and bone matrices behind.
  MemFree(model.currentPose);
  MemFree(model.boneMatrices);
}
