#include "server/stories.h"

#include <optional>

#include "gtest/gtest.h"

namespace nanommo {
namespace {

TEST(Stories, PromptsWithThreeWords) {
  EXPECT_EQ(StoryPrompt("Dragon castle gold"),
            "Words: dragon, castle, gold\nStory:");
  EXPECT_EQ(StoryPrompt("sword,  skeleton , brave, gate"),
            "Words: sword, skeleton, brave\nStory:");
  EXPECT_EQ(StoryPrompt("ash"), "Words: ash\nStory:");
  EXPECT_EQ(StoryPrompt(" , "), std::nullopt);
}

}  // namespace
}  // namespace nanommo
