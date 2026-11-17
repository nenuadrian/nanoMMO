#include "client/renderer.h"

#include <memory>

#include "absl/status/statusor.h"
#include "client/atmosphere.h"
#include "client/offscreen.h"
#include "client/telegraphs.h"
#include "client/window.h"
#include "gtest/gtest.h"
#include "terrain/terrain.h"

TEST(Renderer, ClearsToTheBackgroundColor) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});

  target.Render(renderer);

  const nanommo::Pixel expected{30, 144, 255, 255};
  EXPECT_EQ(target.ReadPixel(0, 0), expected);
  EXPECT_EQ(target.ReadPixel(63, 63), expected);
}

TEST(Renderer, DrawsACubeInTheMiddle) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  renderer.set_cubes({{.color = Color{255, 0, 0, 255}}});

  target.Render(renderer);

  const nanommo::Pixel middle = target.ReadPixel(32, 32);
  EXPECT_GT(middle.r, 100);
  EXPECT_EQ(middle.g, 0);
  EXPECT_EQ(middle.b, 0);
  EXPECT_EQ(target.ReadPixel(0, 0), (nanommo::Pixel{30, 144, 255, 255}));
}

TEST(Renderer, DrawsTheTerrainBelowTheSky) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  renderer.set_terrain(nanommo::Terrain(1), 0);
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  // Looking out to sea, level with the horizon.
  renderer.set_camera({.position = {0.0f, 10.0f, 0.0f},
                       .target = {0.0f, 10.0f, 100.0f},
                       .up = {0.0f, 1.0f, 0.0f},
                       .fovy = 50.0f,
                       .projection = CAMERA_PERSPECTIVE});

  target.Render(renderer);

  // The sky, which goes from the background at the horizon to the deeper blue
  // overhead, so it's blue, but not the background's own colour up there.
  const nanommo::Pixel sky = target.ReadPixel(32, 2);
  EXPECT_GT(sky.b, sky.r + 100);
  EXPECT_NE(sky, (nanommo::Pixel{30, 144, 255, 255}));
  EXPECT_NE(target.ReadPixel(32, 48), sky);
  // The sea, and of the island's 256 squares, the few ahead of us.
  EXPECT_GT(renderer.work().draws, 1);
  EXPECT_LT(renderer.work().draws, 32);
}

TEST(Renderer, RendersAPreviewIntoATexture) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  const Camera3D camera{
      .position = {0.0f, 0.9f, 4.0f},
      .target = {0.0f, 0.9f, 0.0f},
      .up = {0.0f, 1.0f, 0.0f},
      .fovy = 35.0f,
      .projection = CAMERA_PERSPECTIVE,
  };

  target.RenderPreview(renderer, {}, camera);

  EXPECT_NE(target.ReadPixel(32, 32), target.ReadPixel(0, 0));
}

TEST(Renderer, DrawsAMonstersSkillOnTheGroundTheWayItWasAimed) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  const nanommo::Terrain terrain(1);
  renderer.set_terrain(terrain, 0);
  // High over the middle of the land, looking down, with z up the screen, and
  // a metre and a half about five pixels.
  const float ground = terrain.Height(0.0f, 0.0f);
  renderer.set_camera({.position = {0.0f, ground + 20.0f, 0.0f},
                       .target = {0.0f, ground, 0.0f},
                       .up = {0.0f, 0.0f, 1.0f},
                       .fovy = 50.0f,
                       .projection = CAMERA_PERSPECTIVE});
  target.Render(renderer);
  const nanommo::Pixel ahead = target.ReadPixel(32, 27);
  const nanommo::Pixel beside = target.ReadPixel(37, 32);
  const nanommo::Pixel behind = target.ReadPixel(32, 37);

  // A cone along z, in white, half filled.
  nanommo::Telegraph sweep{.left = 1.0f};
  sweep.skill.set_shape(nanommo::monsters::CONE);
  sweep.skill.set_size(4.0f);
  sweep.skill.set_arc(90.0f);
  sweep.skill.set_telegraph_ms(2000);
  sweep.skill.set_color(0xFFFFFFFF);
  renderer.set_telegraphs({{1, sweep}});
  target.Render(renderer);

  const nanommo::Pixel lit = target.ReadPixel(32, 27);
  EXPECT_GT(lit.r + lit.g + lit.b, ahead.r + ahead.g + ahead.b + 30);
  EXPECT_EQ(target.ReadPixel(37, 32), beside);
  EXPECT_EQ(target.ReadPixel(32, 37), behind);
}
