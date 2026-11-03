#include "client/characters.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>

#include "absl/status/statusor.h"
#include "client/camera.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "client/window.h"
#include "content/content.h"
#include "gtest/gtest.h"

namespace {

nanommo::Content GamesContent() {
  return *nanommo::ReadContent(NANOMMO_CONTENT);
}

}  // namespace

TEST(Characters, BuildsAPersonForEachOfTheRigsBuilds) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  const Shader shader = LoadShaderFromMemory(nullptr, nullptr);
  const nanommo::Content content = GamesContent();

  const nanommo::Characters people(content.rig, shader, content.figures);

  EXPECT_EQ(people.size(), static_cast<std::size_t>(content.rig.builds_size()));
  // Walking, each foot comes down once a cycle, half a cycle apart.
  const std::optional<nanommo::Gait> gait = people.GaitOf(
      {.id = 1, .to = nanommo::protocol::MOTION_WALK, .to_time = 0.5f});
  ASSERT_TRUE(gait.has_value());
  EXPECT_NEAR(std::fabs(gait->footfalls[1] - gait->footfalls[0]), 0.5f, 0.05f);
}

TEST(Characters, LoadsTheSkeletonWithAWalkMadeFromItsRun) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  const Shader shader = LoadShaderFromMemory(nullptr, nullptr);

  const nanommo::Characters monsters(nanommo::MonstersFolder(),
                                     {"skeleton.glb"}, 1.6f, shader);

  EXPECT_EQ(monsters.size(), 1u);
}

TEST(Characters, DrawsTheSkeletonWithItsArmsDown) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(256, 256);
  nanommo::Content content;
  content.monsters = *nanommo::ParseMonsters(R"pb(
    kinds {
      id: 1
      look { model: "skeleton.glb" height: 1.6 }
    }
  )pb");
  nanommo::Renderer renderer(content);
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  renderer.set_figures({{.id = 1, .monster = true, .kind = 1}});

  target.Render(renderer);

  const auto pixel = [&target](Vector3 at) {
    const Vector2 seen =
        GetWorldToScreenEx(at, nanommo::GameCamera(), 256, 256);
    return target.ReadPixel(seen.x, seen.y);
  };
  const nanommo::Pixel background{30, 144, 255, 255};
  EXPECT_NE(pixel({0.0f, 1.5f, 0.0f}), background);
  // Where its forearm would be, held out to the side at shoulder height.
  EXPECT_EQ(pixel({0.54f, 1.05f, 0.0f}), background);
}

TEST(Characters, DrawsAPlayerStandingInTheMiddle) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer(GamesContent());
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  // In the Linen Shirt, which is dyed in each player's colour.
  renderer.set_figures({{.id = 1, .wearing = {8}}});

  target.Render(renderer);

  // At the chest, the shirt is orange: player one's colour.
  const Vector2 chest =
      GetWorldToScreenEx({0.0f, 1.3f, 0.0f}, nanommo::GameCamera(), 64, 64);
  const nanommo::Pixel body = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(body.r, body.g);
  EXPECT_GT(body.g, body.b);
}

TEST(Characters, DrawsWhatAPlayerWearsOverTheirFigure) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(256, 256);
  // A figure that's just a body, thick enough to cover a few pixels from the
  // game's camera, in a green skin, and a blue tunic, a ball round the
  // chest, on the game's rig.
  nanommo::Content content;
  content.rig = GamesContent().rig;
  content.figures = *nanommo::ParseFigures(R"pb(
    body {
      shape: SHAPE_LIMB
      bone: "Spine"
      from: "Hips"
      to: "Neck"
      start_radius: 0.15
      end_radius: 0.15
      color: 0xFFFFFFFF
    }
    skins: 0x40C040FF
  )pb");
  content.items = *nanommo::ParseItems(R"pb(
    items {
      id: 5
      name: "Tunic"
      equip { slot: SLOT_BODY }
      look {
        parts {
          shape: SHAPE_BALL
          bone: "Spine2"
          size { x: 0.25 y: 0.25 z: 0.25 }
          color: 0x2040E0FF
        }
      }
    }
    items {
      id: 8
      name: "Shirt"
      equip { slot: SLOT_BODY }
      look {
        dyed: true
        parts {
          shape: SHAPE_BALL
          bone: "Spine2"
          size { x: 0.25 y: 0.25 z: 0.25 }
          color: 0xFFFFFFFF
        }
      }
    }
  )pb");
  nanommo::Renderer renderer(content);
  renderer.set_background({0.0f, 0.0f, 0.0f});
  const Vector2 chest =
      GetWorldToScreenEx({0.0f, 1.3f, 0.0f}, nanommo::GameCamera(), 256, 256);

  // Player one is green, in their skin, and in the tunic, they're blue at
  // the chest.
  renderer.set_figures({{.id = 1}});
  target.Render(renderer);
  const nanommo::Pixel bare = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(bare.g, bare.b);
  renderer.set_figures({{.id = 1, .wearing = {5}}});
  target.Render(renderer);
  const nanommo::Pixel dressed = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(dressed.b, dressed.g);

  // The shirt is dyed, so it's in the colour of whoever wears it: player
  // one's is orange, and player two's is blue.
  renderer.set_figures({{.id = 1, .wearing = {8}}});
  target.Render(renderer);
  const nanommo::Pixel orange = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(orange.r, orange.g);
  EXPECT_GT(orange.g, orange.b);
  renderer.set_figures({{.id = 2, .wearing = {8}}});
  target.Render(renderer);
  const nanommo::Pixel blue = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(blue.b, blue.g);
  EXPECT_GT(blue.g, blue.r);
}

TEST(Characters, DrawsADeadPlayerLyingOnTheGroundLongAfterTheFall) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  // From the side and low down, where a body lying on the ground can't be
  // taken for one standing.
  const Camera3D side{.position = {4.0f, 0.8f, 0.0f},
                      .target = {0.0f, 0.6f, 0.0f},
                      .up = {0.0f, 1.0f, 0.0f},
                      .fovy = 45.0f,
                      .projection = CAMERA_PERSPECTIVE};
  const int head =
      static_cast<int>(GetWorldToScreenEx({0.0f, 1.6f, 0.0f}, side, 64, 64).y);
  const int knee =
      static_cast<int>(GetWorldToScreenEx({0.0f, 0.5f, 0.0f}, side, 64, 64).y);
  // The highest row of the picture's middle column the figure covers, after
  // dying so long ago, blending in from the idle as the animator does.
  auto top = [&](float since) {
    nanommo::Offscreen target(64, 64);
    nanommo::Renderer renderer(GamesContent());
    renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
    renderer.set_camera(side);
    renderer.set_figures({{.id = 1,
                           .wearing = {8},
                           .from = nanommo::protocol::MOTION_IDLE,
                           .from_time = since,
                           .to = nanommo::protocol::MOTION_DEAD,
                           .to_time = since,
                           .blend = std::min(since / 0.25f, 1.0f)}});
    target.Render(renderer);
    for (int y = 0; y < 64; ++y) {
      const nanommo::Pixel pixel = target.ReadPixel(32, y);
      if (pixel.b < 200 || pixel.r > 60) {
        return y;
      }
    }
    return 64;
  };

  // As it's struck, it stands, its head as high as a head.
  EXPECT_LE(top(0.0f), head);
  // Once the fall is over, and long after, it lies below the knees.
  EXPECT_GT(top(3.0f), knee);
  EXPECT_GT(top(10.0f), knee);
}
