#include "protocol/framing.h"

#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

nanommo::protocol::Input MakeInput(float x, float z) {
  nanommo::protocol::Input message;
  message.set_x(x);
  message.set_z(z);
  return message;
}

float XOf(const std::string& bytes) {
  nanommo::protocol::Input message;
  EXPECT_TRUE(message.ParseFromString(bytes));
  return message.x();
}

}  // namespace

TEST(Framing, WritesTheSizeMostSignificantByteFirst) {
  EXPECT_EQ(nanommo::Header(258), std::string("\0\0\x01\x02", 4));
}

TEST(Framing, WaitsForTheWholeMessage) {
  const std::string bytes = nanommo::Frame(MakeInput(1.0f, -1.0f));
  nanommo::FrameReader reader;

  reader.Append(bytes.substr(0, 6));
  EXPECT_FALSE(reader.Next());

  reader.Append(bytes.substr(6));
  const auto message = reader.Next();
  ASSERT_TRUE(message);
  EXPECT_EQ(XOf(*message), 1.0f);
}

TEST(Framing, SplitsMessagesThatArriveTogether) {
  nanommo::FrameReader reader;
  reader.Append(nanommo::Frame(MakeInput(1.0f, 0.0f)) +
                nanommo::Frame(MakeInput(-1.0f, 0.0f)));

  const auto first = reader.Next();
  const auto second = reader.Next();
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(XOf(*first), 1.0f);
  EXPECT_EQ(XOf(*second), -1.0f);
  EXPECT_FALSE(reader.Next());
}

TEST(Framing, RejectsMessagesThatAreTooLarge) {
  nanommo::FrameReader reader;
  reader.Append(std::string("\xff\xff\xff\xff", 4));

  EXPECT_FALSE(reader.Next());
  EXPECT_EQ(reader.status().code(), absl::StatusCode::kInvalidArgument);
}
