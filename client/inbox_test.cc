#include "client/inbox.h"

#include <string>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {
namespace {

TEST(Inbox, FilesEachKindOfMessageWhereTheGameReadsIt) {
  Inbox inbox;
  protocol::World chat;
  chat.add_chat()->set_text("hello");
  protocol::World bag;
  bag.mutable_inventory()->set_gold(7);
  protocol::World events;
  events.add_events();
  protocol::World move;
  move.mutable_move()->set_port(7778);
  for (const protocol::World& message : {chat, bag, events, move}) {
    ASSERT_TRUE(inbox.Take(message.SerializeAsString()).ok());
  }
  ASSERT_EQ(inbox.chat().size(), 1u);
  EXPECT_EQ(inbox.chat()[0].text(), "hello");
  EXPECT_EQ(inbox.inventory().gold(), 7u);
  EXPECT_EQ(inbox.TakeEvents().size(), 1u);
  // Events come once.
  EXPECT_TRUE(inbox.TakeEvents().empty());
  ASSERT_TRUE(inbox.move().has_value());
  EXPECT_EQ(inbox.move()->port(), 7778u);
}

TEST(Inbox, RefusesWhatIsntAMessage) {
  Inbox inbox;
  EXPECT_FALSE(inbox.Take(std::string("\xff\xff\xff\xff", 4)).ok());
  EXPECT_TRUE(inbox.Take("").ok());
}

}  // namespace
}  // namespace nanommo
