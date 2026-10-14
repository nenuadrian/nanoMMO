#include "server/interest.h"

#include <cstdint>
#include <map>
#include <vector>

#include "gtest/gtest.h"
#include "server/world.h"

TEST(Interest, SendsEachPlayerWhatIsNearIt) {
  const std::map<uint32_t, nanommo::Player> players = {
      {1, {.x = 5.0f, .z = 5.0f}},
      {2, {.x = 60.0f, .z = -40.0f}},
      {3, {.x = 200.0f, .z = 5.0f}},
  };
  const std::map<uint32_t, nanommo::Monster> monsters = {
      {7, {.x = -20.0f, .z = 10.0f}},
  };

  const std::map<nanommo::Cell, nanommo::View> views =
      nanommo::Views(players, monsters);

  ASSERT_EQ(views.size(), 3u);
  const nanommo::View& first = views.at(nanommo::CellOf(5.0f, 5.0f));
  EXPECT_EQ(first.here, (std::vector<uint32_t>{1}));
  EXPECT_EQ(first.players, (std::vector<uint32_t>{2, 1}));
  EXPECT_EQ(first.monsters, (std::vector<uint32_t>{7}));
  const nanommo::View& far = views.at(nanommo::CellOf(200.0f, 5.0f));
  EXPECT_EQ(far.players, (std::vector<uint32_t>{3}));
  EXPECT_TRUE(far.monsters.empty());
}
