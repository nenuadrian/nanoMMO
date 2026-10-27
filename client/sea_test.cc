#include "client/sea.h"

#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "client/atmosphere.h"
#include "client/window.h"
#include "gtest/gtest.h"

namespace {

// Out to sea, north of the Meadows' island, looking down at the water.
constexpr Camera3D kOutToSea{.position = {0.0f, 6.0f, -200.0f},
                             .target = {0.0f, 0.0f, -215.0f},
                             .up = {0.0f, 1.0f, 0.0f},
                             .fovy = 50.0f,
                             .projection = CAMERA_PERSPECTIVE};

// The sea alone, as it is a number of seconds in, as each pixel's colour.
std::vector<Color> Picture(const nanommo::Sea& sea, float seconds) {
  const RenderTexture2D target = LoadRenderTexture(64, 64);
  BeginTextureMode(target);
  ClearBackground(BLACK);
  BeginMode3D(kOutToSea);
  sea.Draw(kOutToSea, nanommo::kMeadows.atmosphere, 0.0f, seconds);
  EndMode3D();
  EndTextureMode();
  Image image = LoadImageFromTexture(target.texture);
  Color* colors = LoadImageColors(image);
  std::vector<Color> picture(colors, colors + 64 * 64);
  UnloadImageColors(colors);
  UnloadImage(image);
  UnloadRenderTexture(target);
  return picture;
}

bool Match(const std::vector<Color>& a, const std::vector<Color>& b) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(Sea, MovesWithTheTime) {
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  const nanommo::Sea sea(nanommo::kMeadows);

  const std::vector<Color> now = Picture(sea, 10.0f);

  EXPECT_TRUE(Match(now, Picture(sea, 10.0f)));
  EXPECT_FALSE(Match(now, Picture(sea, 10.5f)));
}
