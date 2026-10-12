#include "client/renderer.h"

#include <memory>

#include "absl/status/statusor.h"
#include "client/offscreen.h"
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
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  renderer.set_terrain(nanommo::Terrain(1));
  // Looking out to sea, level with the horizon.
  renderer.set_camera({.position = {0.0f, 10.0f, 0.0f},
                       .target = {0.0f, 10.0f, 100.0f},
                       .up = {0.0f, 1.0f, 0.0f},
                       .fovy = 50.0f,
                       .projection = CAMERA_PERSPECTIVE});

  target.Render(renderer);

  const nanommo::Pixel background{30, 144, 255, 255};
  EXPECT_EQ(target.ReadPixel(32, 2), background);
  EXPECT_NE(target.ReadPixel(32, 48), background);
  // The sea, and of the island's 256 squares, the few ahead of us.
  EXPECT_GT(renderer.work().draws, 1);
  EXPECT_LT(renderer.work().draws, 32);
}
