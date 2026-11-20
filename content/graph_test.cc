#include "content/graph.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/content.h"
#include "gtest/gtest.h"
#include "smollergraph/graph.h"

namespace nanommo {
namespace {

Content GamesContent() {
  absl::StatusOr<Content> content = ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(content.ok()) << content.status();
  return *std::move(content);
}

// What Unreachable finds in the content, a line each, or the error, as the
// one line.
std::vector<std::string> Problems(const Content& content) {
  smollergraph::Graph graph;
  if (absl::Status added = AddContentGraph(content, graph); !added.ok()) {
    return {std::string(added.message())};
  }
  absl::StatusOr<std::vector<std::string>> lines = Unreachable(graph);
  if (!lines.ok()) return {std::string(lines.status().message())};
  return *std::move(lines);
}

TEST(ContentGraph, ANewPlayerCanReachAllOfTheGame) {
  EXPECT_EQ(Problems(GamesContent()), std::vector<std::string>{});
}

TEST(ContentGraph, FindsAQuestNobodyCanTake) {
  // The Guard gives Bones by the Gate. Taken out of the Meadows, he stands
  // nowhere, which CheckContent is fine with, as he's still in the NPCs file.
  Content content = GamesContent();
  std::erase_if(content.npcs.spawns, [&](const npcs::Spawn& spawn) {
    return content.npcs.kinds.at(spawn.kind()).name() == "Guard";
  });
  EXPECT_EQ(CheckContent(content), absl::OkStatus());
  EXPECT_EQ(Problems(content), (std::vector<std::string>{
                                   "Guard, an NPC",
                                   "Bones by the Gate, a quest",
                               }));
}

TEST(ContentGraph, FindsAQuestNobodyCanFinish) {
  // The Crypt's skeletons live in camps there and nowhere else.
  Content content = GamesContent();
  std::erase_if(content.monsters.camps, [&](const monsters::Camps& camps) {
    return content.monsters.kinds.at(camps.kind()).name() == "Crypt Skeleton";
  });
  EXPECT_EQ(Problems(content),
            (std::vector<std::string>{
                "Crypt Skeleton, a monster",
                "Rest for the Crypt asks for the Crypt Skeleton, which no new "
                "player can reach",
            }));
}

TEST(ContentGraph, FindsAnItemNothingGives) {
  // The Skeleton is all that drops the Leather Cap.
  Content content = GamesContent();
  for (auto& [id, kind] : content.monsters.kinds) {
    auto* loot = kind.mutable_loot();
    loot->erase(
        std::remove_if(loot->begin(), loot->end(),
                       [&](const monsters::Drop& drop) {
                         return content.items.by_id.at(drop.item()).name() ==
                                "Leather Cap";
                       }),
        loot->end());
  }
  EXPECT_EQ(Problems(content),
            std::vector<std::string>{"Leather Cap, an item"});
}

TEST(ContentGraph, RefusesTwoThingsWithOneName) {
  // An item called after an NPC would be one node, and the graph would have
  // the Guard dropped by skeletons.
  Content content = GamesContent();
  content.items.by_id.begin()->second.set_name("Guard");
  EXPECT_EQ(Problems(content),
            std::vector<std::string>{"two things are called Guard"});
}

}  // namespace
}  // namespace nanommo
