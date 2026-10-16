#include "client/characters.h"

#include <memory>

#include "absl/status/statusor.h"
#include "client/camera.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "client/window.h"
#include "gtest/gtest.h"

namespace {

bool HaveCharacters() {
  return FileExists((nanommo::CharactersFolder() + "/x_bot.glb").c_str());
}

}  // namespace

TEST(Characters, LoadsBothBotsWithTheirClips) {
  if (!HaveCharacters()) {
    GTEST_SKIP() << "no characters: see assets/mixamo/README.md";
  }
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  const Shader shader = LoadShaderFromMemory(nullptr, nullptr);

  const nanommo::Characters characters(
      nanommo::CharactersFolder(), {"x_bot.glb", "y_bot.glb"}, 1.8f, shader);

  EXPECT_EQ(characters.size(), 2u);
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
  if (!HaveCharacters()) {
    GTEST_SKIP() << "no characters: see assets/mixamo/README.md";
  }
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  nanommo::Offscreen target(64, 64);
  nanommo::Renderer renderer;
  renderer.set_background({30 / 255.0f, 144 / 255.0f, 255 / 255.0f});
  renderer.set_figures({{.id = 1}});

  target.Render(renderer);

  // Chest height, higher than a cube reaches, is orange: player one's colour.
  const Vector2 chest =
      GetWorldToScreenEx({0.0f, 1.4f, 0.0f}, nanommo::GameCamera(), 64, 64);
  const nanommo::Pixel body = target.ReadPixel(chest.x, chest.y);
  EXPECT_GT(body.r, body.g);
  EXPECT_GT(body.g, body.b);
}
