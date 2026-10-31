#include "client/map.h"

#include <raylib.h>

#include "client/atmosphere.h"
#include "gtest/gtest.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace {

Color PixelAt(const Image& image, int across, int down) {
  return static_cast<const Color*>(image.data)[down * image.width + across];
}

bool Same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

}  // namespace

TEST(Map, DrawsTheLandFromAboveAPixelAMetre) {
  const nanommo::Terrain island(1);
  Image image = nanommo::DrawLand(island, 0);
  ASSERT_EQ(image.width, 512);
  ASSERT_EQ(image.height, 512);

  // The corner is deep sea, and the middle, where players start, is land.
  EXPECT_TRUE(Same(PixelAt(image, 0, 0), nanommo::kMeadows.sea.deep));
  EXPECT_FALSE(Same(PixelAt(image, 256, 256), nanommo::kMeadows.sea.deep));
  EXPECT_FALSE(Same(PixelAt(image, 256, 256), nanommo::kMeadows.sea.shallow));

  // A tree is a dark spot where it stands.
  const nanommo::Tree& tree = island.trees().front();
  const Color spot = PixelAt(image, static_cast<int>(tree.x + 256.0f),
                             static_cast<int>(tree.z + 256.0f));
  EXPECT_LT(spot.r + spot.g + spot.b, 3 * 100);
  UnloadImage(image);
}
