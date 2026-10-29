#include "client/picking.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace {

nanommo::Figure Npc(uint32_t id, float z, bool talks) {
  nanommo::Figure npc;
  npc.id = id;
  npc.npc = true;
  npc.talks = talks;
  npc.position = {0.0f, 0.0f, z};
  return npc;
}

}  // namespace

TEST(Picking, PicksTheNearestNpcThatTalksOrTeaches) {
  std::vector<nanommo::Figure> figures = {
      Npc(1, -10.0f, true), Npc(2, -5.0f, false), Npc(3, -7.0f, true)};
  const Ray ahead{.position = {0.0f, 0.5f, 0.0f},
                  .direction = {0.0f, 0.0f, -1.0f}};
  const Ray beside{.position = {2.0f, 0.5f, 0.0f},
                   .direction = {0.0f, 0.0f, -1.0f}};

  ASSERT_NE(nanommo::PickNpc(figures, ahead), nullptr);
  EXPECT_EQ(nanommo::PickNpc(figures, ahead)->id, 3u);
  EXPECT_EQ(nanommo::PickNpc(figures, beside), nullptr);

  // A trainer is picked too, though it doesn't talk, and so is an NPC who
  // gives quests.
  figures.push_back(Npc(4, -6.0f, false));
  figures.back().teaches = {2};
  EXPECT_EQ(nanommo::PickNpc(figures, ahead)->id, 4u);
  figures.push_back(Npc(5, -5.5f, false));
  figures.back().quests = {1};
  EXPECT_EQ(nanommo::PickNpc(figures, ahead)->id, 5u);
}
