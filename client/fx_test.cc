#include "client/fx.h"

#include <memory>

#include "absl/status/statusor.h"
#include "client/animator.h"
#include "client/window.h"
#include "content/skills.h"
#include "gtest/gtest.h"

TEST(Fx, LastUntilTheSkillsFxAreOver) {
  // The fx load a shader, which needs a window.
  const absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("test", 64, 64,
                              nanommo::Window::Visibility::kHidden);
  ASSERT_TRUE(window.ok());
  const absl::StatusOr<nanommo::SkillTable> skills = nanommo::ParseSkills(R"pb(
    skills {
      id: 1
      name: "Glow"
      level: 1
      effect { cast_ms: 500 }
      look { fx { kind: GLOW from_ms: 200 for_ms: 800 size: 1 } }
    }
  )pb");
  ASSERT_TRUE(skills.ok());
  nanommo::Fx fx(*skills);
  nanommo::Figure player{
      .id = 1, .to = nanommo::protocol::MOTION_SKILL, .to_skill = 1};

  fx.Update({player}, 0.0f);
  EXPECT_EQ(fx.casts(), 1u);

  // The skill's over after half a second, and its glow a second in.
  player.to = nanommo::protocol::MOTION_IDLE;
  player.to_skill = 0;
  fx.Update({player}, 0.6f);
  EXPECT_EQ(fx.casts(), 1u);
  fx.Update({player}, 0.6f);
  EXPECT_EQ(fx.casts(), 0u);
}
