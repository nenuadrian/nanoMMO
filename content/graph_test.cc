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

// What Unreachable and Cycles find in the content, a line each, or the error,
// as the one line.
std::vector<std::string> Problems(const Content& content) {
  smollergraph::Graph graph;
  if (absl::Status added = AddContentGraph(content, graph); !added.ok()) {
    return {std::string(added.message())};
  }
  absl::StatusOr<std::vector<std::string>> lines = Unreachable(graph);
  if (!lines.ok()) return {std::string(lines.status().message())};
  absl::StatusOr<std::vector<std::string>> cycles = Cycles(graph);
  if (!cycles.ok()) return {std::string(cycles.status().message())};
  lines->insert(lines->end(), cycles->begin(), cycles->end());
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

TEST(ContentGraph, ARecipeNeedsAllItTakes) {
  // Binding a Bone Sword takes a Silver Ingot too, which nothing drops, so a
  // new player can't, though they start with the sword and skeletons drop
  // bones. Nor can they temper an Ash Blade, which takes a Bone Sword, though
  // they can still find one on the Ashlands' skeletons.
  Content content = GamesContent();
  content.items.by_id[99].set_id(99);
  content.items.by_id[99].set_name("Silver Ingot");
  recipes::ItemCount* ingot = content.recipes.at(2).add_inputs();
  ingot->set_item(99);
  ingot->set_count(1);
  EXPECT_EQ(CheckContent(content), absl::OkStatus());
  EXPECT_EQ(Problems(content), (std::vector<std::string>{
                                   "Bone Sword, an item",
                                   "Silver Ingot, an item",
                                   "Bind a Bone Sword, a recipe",
                                   "Temper an Ash Blade, a recipe",
                               }));

  // Once the Skeleton drops ingots, a new player can make both.
  monsters::Drop* drop = content.monsters.kinds.at(1).add_loot();
  drop->set_item(99);
  drop->set_chance(0.1f);
  EXPECT_EQ(Problems(content), std::vector<std::string>{});
}

TEST(ContentGraph, RefusesAnItemThatGoesBackIntoItself) {
  // Breaking a Bone Sword gives a Leather Cap, which goes into nothing.
  Content content = GamesContent();
  recipes::Recipe& breaks = content.recipes[4];
  breaks.set_id(4);
  breaks.set_name("Break a Bone Sword");
  recipes::ItemCount* sword = breaks.add_inputs();
  sword->set_item(13);
  sword->set_count(1);
  breaks.mutable_output()->set_item(4);
  breaks.mutable_output()->set_count(1);
  EXPECT_EQ(Problems(content), std::vector<std::string>{});

  // Twenty bones back from it, for the ten that bind it, would make bones from
  // nothing, ten at a time, which CheckContent and Unreachable are fine with.
  breaks.mutable_output()->set_item(3);
  breaks.mutable_output()->set_count(20);
  EXPECT_EQ(CheckContent(content), absl::OkStatus());
  EXPECT_EQ(Problems(content),
            (std::vector<std::string>{
                "Bone goes back into itself: (Bone)-[:INTO]->(Bind a Bone "
                "Sword)-[:MAKES]->(Bone Sword)-[:INTO]->(Break a Bone "
                "Sword)-[:MAKES]->(Bone)",
                "Bone Sword goes back into itself: (Bone Sword)-[:INTO]->"
                "(Break a Bone Sword)-[:MAKES]->(Bone)-[:INTO]->(Bind a Bone "
                "Sword)-[:MAKES]->(Bone Sword)",
            }));
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
