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
  npc.radius = 0.5f;
  npc.position = {0.0f, 0.0f, z};
  return npc;
}

}  // namespace

TEST(Picking, PicksTheNearestNpcThatTalks) {
  const std::vector<nanommo::Figure> figures = {
      Npc(1, -10.0f, true), Npc(2, -5.0f, false), Npc(3, -7.0f, true)};
  const Ray ahead{.position = {0.0f, 0.5f, 0.0f},
                  .direction = {0.0f, 0.0f, -1.0f}};
  const Ray beside{.position = {2.0f, 0.5f, 0.0f},
                   .direction = {0.0f, 0.0f, -1.0f}};

  ASSERT_NE(nanommo::PickNpc(figures, ahead), nullptr);
  EXPECT_EQ(nanommo::PickNpc(figures, ahead)->id, 3u);
  EXPECT_EQ(nanommo::PickNpc(figures, beside), nullptr);
}
