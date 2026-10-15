#include "server/stories.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "protocol/chat.h"

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

TEST(Stories, BreaksAStoryIntoLinesOfChat) {
  std::string story;
  for (int i = 0; i < 10; ++i) {
    story += "The brave knight found a gold sword by the river. ";
  }
  const std::vector<std::string> lines = StoryLines(story + "Then he");

  EXPECT_EQ(lines.size(), 3u);
  for (const std::string& line : lines) {
    EXPECT_LE(line.size(), kMaxChatLength);
    EXPECT_TRUE(line.ends_with("river."));
  }
  EXPECT_EQ(absl::StrJoin(lines, " ") + " ", story);
  EXPECT_EQ(StoryLines("Tom said, \"Can I come?\" Then he"),
            std::vector<std::string>{"Tom said, \"Can I come?\""});
}

}  // namespace
}  // namespace nanommo
