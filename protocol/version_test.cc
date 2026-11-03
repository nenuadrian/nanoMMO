#include "protocol/version.h"

#include "gtest/gtest.h"

TEST(Version, APatchIsCompatible) {
  EXPECT_TRUE(nanommo::Compatible("0.59.0", "0.59.0"));
  EXPECT_TRUE(nanommo::Compatible("0.59.2", "0.59.0"));
  EXPECT_TRUE(nanommo::Compatible("0.59.0", "0.59.2"));
}

TEST(Version, AnotherMinorOrMajorVersionIsNot) {
  EXPECT_FALSE(nanommo::Compatible("0.58.0", "0.59.0"));
  EXPECT_FALSE(nanommo::Compatible("0.60.0", "0.59.0"));
  EXPECT_FALSE(nanommo::Compatible("1.59.0", "0.59.0"));
}

TEST(Version, NorIsWhatIsntAVersion) {
  EXPECT_FALSE(nanommo::Compatible("", "0.59.0"));
  EXPECT_FALSE(nanommo::Compatible("0.59", "0.59.0"));
  EXPECT_FALSE(nanommo::Compatible("0.59.x", "0.59.0"));
}

// Set to 0.60 in CMakeLists.txt, without its patch, it would turn every
// client away, its own too.
TEST(Version, OursIsAVersion) {
  EXPECT_TRUE(nanommo::Compatible(nanommo::kVersion, nanommo::kVersion));
}
