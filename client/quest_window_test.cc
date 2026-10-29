#include "client/quest_window.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace {

nanommo::Figure Giver(uint32_t id, std::vector<uint32_t> quests) {
  nanommo::Figure npc;
  npc.id = id;
  npc.npc = true;
  npc.quests = std::move(quests);
  return npc;
}

}  // namespace

TEST(QuestGivers, MarkTheQuestsToTakeAndToTurnIn) {
  nanommo::Content content;
  content.quests = *nanommo::ParseQuests(R"pb(
    quests {
      id: 1
      giver: 1
      kill { monster: 1 count: 3 }
    }
    quests {
      id: 2
      giver: 1
      kill { monster: 1 count: 5 }
    }
    quests {
      id: 3
      giver: 2
      kill { monster: 2 count: 5 }
    }
  )pb");
  std::vector<nanommo::Figure> figures = {Giver(1, {1, 2}), Giver(2, {3})};
  nanommo::protocol::Quests yours;

  // Nothing taken: each has one to take.
  nanommo::MarkQuestGivers(figures, content, yours);
  EXPECT_EQ(figures[0].mark, '!');
  EXPECT_EQ(figures[1].mark, '!');

  // On the second's only quest, not done: nothing to take or turn in.
  (*yours.mutable_on())[3] = 2;
  nanommo::MarkQuestGivers(figures, content, yours);
  EXPECT_EQ(figures[1].mark, 0);

  // The first quest's kills are in, which is to turn in, over the second
  // quest, to take.
  (*yours.mutable_on())[1] = 3;
  nanommo::MarkQuestGivers(figures, content, yours);
  EXPECT_EQ(figures[0].mark, '?');

  // Turned in, it's done, and the second's left to take.
  yours.mutable_on()->erase(1);
  yours.add_done(1);
  nanommo::MarkQuestGivers(figures, content, yours);
  EXPECT_EQ(figures[0].mark, '!');
}
