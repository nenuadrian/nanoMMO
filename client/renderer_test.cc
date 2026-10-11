#include "client/renderer.h"

#include <memory>

#include "absl/status/statusor.h"
#include "client/offscreen.h"
#include "client/window.h"
#include "gtest/gtest.h"

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
