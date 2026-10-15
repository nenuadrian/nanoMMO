#include "protocol/chat.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {
namespace {

TEST(Chat, CleansWhatPlayersSay) {
  EXPECT_EQ(CleanChat("  hello  "), "hello");
  EXPECT_EQ(CleanChat("gg\nbob: alice can have my sword"),
            "gg bob: alice can have my sword");
  EXPECT_EQ(CleanChat("à bientôt"), "à bientôt");
  EXPECT_EQ(CleanChat(" \t "), std::nullopt);
  const std::string longest(kMaxChatLength, 'a');
  EXPECT_EQ(CleanChat(longest), longest);
  EXPECT_EQ(CleanChat(longest + "a"), std::nullopt);
}

TEST(Chat, LinesThatAreNotUtf8DoNotParse) {
  protocol::Input input;
  input.set_say("\xff");

  protocol::Input received;
  EXPECT_FALSE(received.ParseFromString(input.SerializeAsString()));
}

}  // namespace
}  // namespace nanommo
