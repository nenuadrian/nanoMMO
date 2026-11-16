#include "gateway/websocket.h"

#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace nanommo {
namespace {

constexpr char kKey[] = "dGhlIHNhbXBsZSBub25jZQ==";

std::string Request(const std::string& headers) {
  return absl::StrCat("GET /login HTTP/1.1\r\nHost: localhost:8090\r\n",
                      headers, "\r\n");
}

constexpr char kUpgrade[] =
    "Upgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n"
    "Sec-WebSocket-Version: 13\r\n";

// Why the handshake turns the request down.
std::string Refusal(const std::string& request) {
  return std::string(ParseHandshake(request).status().message());
}

TEST(WebSocket, AcceptsTheKeyAsTheRfcsExampleDoes) {
  // RFC 6455, section 1.3.
  EXPECT_EQ(AcceptKey(kKey), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
  EXPECT_NE(Accept(kKey).find("Sec-WebSocket-Accept: "
                              "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"),
            std::string::npos);
}

TEST(WebSocket, ReadsAHandshakeAndRefusesWhatIsntOne) {
  const absl::StatusOr<Handshake> handshake = ParseHandshake(
      Request(absl::StrCat(kUpgrade, "Sec-WebSocket-Key: ", kKey, "\r\n")));
  ASSERT_TRUE(handshake.ok()) << handshake.status();
  EXPECT_EQ(handshake->path, "/login");
  EXPECT_EQ(handshake->key, kKey);

  // The handshake above with one thing changed each time, refused for it.
  EXPECT_EQ(Refusal(Request(absl::StrCat("Sec-WebSocket-Version: 13\r\n",
                                         "Sec-WebSocket-Key: ", kKey, "\r\n"))),
            "not an upgrade to websocket");
  EXPECT_EQ(
      Refusal(Request(absl::StrCat(
          "Upgrade: websocket\r\nConnection: Upgrade\r\n",
          "Sec-WebSocket-Version: 8\r\nSec-WebSocket-Key: ", kKey, "\r\n"))),
      "not WebSocket version 13");
  EXPECT_EQ(Refusal(Request(
                absl::StrCat(kUpgrade, "Sec-WebSocket-Key: c2hvcnQ=\r\n"))),
            "no Sec-WebSocket-Key of 16 bytes");
  EXPECT_EQ(Refusal(absl::StrCat("POST /login HTTP/1.1\r\n", kUpgrade,
                                 "Sec-WebSocket-Key: ", kKey, "\r\n\r\n")),
            "not an HTTP/1.1 GET");
}

TEST(WebSocket, ReadsAMaskedMessageBackAsItWasSent) {
  FrameDecoder decoder(1024);
  decoder.Append(ClientFrame(Opcode::kBinary, "a game message", 0x12345678));
  const std::optional<Message> message = decoder.Next();
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->opcode, Opcode::kBinary);
  EXPECT_EQ(message->payload, "a game message");
  EXPECT_FALSE(decoder.Next().has_value());
  EXPECT_TRUE(decoder.status().ok());
}

TEST(WebSocket, WaitsForAFrameThatComesInPieces) {
  // 300 bytes takes the two-byte length.
  const std::string payload(300, 'x');
  const std::string frame = ClientFrame(Opcode::kBinary, payload, 0xA1B2C3D4);
  FrameDecoder decoder(1024);
  for (const char c : frame.substr(0, frame.size() - 1)) {
    decoder.Append(std::string(1, c));
    EXPECT_FALSE(decoder.Next().has_value());
  }
  decoder.Append(frame.substr(frame.size() - 1));
  const std::optional<Message> message = decoder.Next();
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->payload, payload);
}

TEST(WebSocket, PutsAMessageTogetherFromItsFragmentsAroundAPing) {
  FrameDecoder decoder(1024);
  decoder.Append(ClientFrame(Opcode::kBinary, "one, ", 1, /*fin=*/false));
  decoder.Append(ClientFrame(Opcode::kPing, "are you there", 2));
  decoder.Append(ClientFrame(Opcode::kContinuation, "two, ", 3, false));
  decoder.Append(ClientFrame(Opcode::kContinuation, "three", 4));
  const std::optional<Message> ping = decoder.Next();
  ASSERT_TRUE(ping.has_value());
  EXPECT_EQ(ping->opcode, Opcode::kPing);
  EXPECT_EQ(ping->payload, "are you there");
  const std::optional<Message> message = decoder.Next();
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->opcode, Opcode::kBinary);
  EXPECT_EQ(message->payload, "one, two, three");
}

TEST(WebSocket, RefusesAnUnmaskedFrameATooLargeMessageAndAStrayContinuation) {
  // A server's frame, as a client mustn't send one.
  FrameDecoder unmasked(1024);
  unmasked.Append(ServerFrame(Opcode::kBinary, "hello"));
  EXPECT_FALSE(unmasked.Next().has_value());
  EXPECT_EQ(unmasked.status().message(), "a client's frame has to be masked");

  // 11 bytes against a limit of 10, as two fragments, and its twin of 10.
  FrameDecoder large(10);
  large.Append(ClientFrame(Opcode::kBinary, "123456", 1, false));
  large.Append(ClientFrame(Opcode::kContinuation, "78901", 2));
  EXPECT_FALSE(large.Next().has_value());
  EXPECT_EQ(large.status().message(), "a message over 10 bytes");
  FrameDecoder fits(10);
  fits.Append(ClientFrame(Opcode::kBinary, "123456", 1, false));
  fits.Append(ClientFrame(Opcode::kContinuation, "7890", 2));
  EXPECT_TRUE(fits.Next().has_value());

  FrameDecoder stray(1024);
  stray.Append(ClientFrame(Opcode::kContinuation, "more", 1));
  EXPECT_FALSE(stray.Next().has_value());
  EXPECT_EQ(stray.status().message(), "a continuation of no message");
}

TEST(WebSocket, WritesTheServersFramesWithTheirLengths) {
  EXPECT_EQ(ServerFrame(Opcode::kBinary, "hi"), std::string("\x82\x02hi", 4));
  const std::string medium =
      ServerFrame(Opcode::kBinary, std::string(300, 'x'));
  EXPECT_EQ(medium.substr(0, 4), std::string("\x82\x7e\x01\x2c", 4));
  const std::string large =
      ServerFrame(Opcode::kBinary, std::string(70000, 'x'));
  EXPECT_EQ(large.substr(0, 10),
            std::string("\x82\x7f\x00\x00\x00\x00\x00\x01\x11\x70", 10));
}

}  // namespace
}  // namespace nanommo
