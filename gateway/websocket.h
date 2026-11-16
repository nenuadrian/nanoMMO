#ifndef NANOMMO_GATEWAY_WEBSOCKET_H_
#define NANOMMO_GATEWAY_WEBSOCKET_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace nanommo {

// The server's side of WebSocket (RFC 6455): what a browser's page opens to
// the gateway, which carries the game's own messages to the servers.

/// What a client's opening handshake asks for.
struct Handshake {
  std::string path;
  std::string key;
};

/// The client's opening handshake, an HTTP GET up to its blank line, or why it
/// isn't one: the upgrade to websocket, version 13 and a key, as RFC 6455 has
/// them.
absl::StatusOr<Handshake> ParseHandshake(std::string_view request);

/// The value of Sec-WebSocket-Accept for the client's key: the SHA-1 of the
/// key and the protocol's GUID, in base 64, which shows the client that the
/// server read its handshake.
std::string AcceptKey(std::string_view key);

/// The server's answer that switches the connection over to WebSocket.
std::string Accept(std::string_view key);

/// An HTTP answer that turns the connection down, with why in its body.
std::string Refuse(int status, std::string_view why);

enum class Opcode : uint8_t {
  kContinuation = 0x0,
  kText = 0x1,
  kBinary = 0x2,
  kClose = 0x8,
  kPing = 0x9,
  kPong = 0xA,
};

/// A whole message, or a ping, a pong or a close.
struct Message {
  Opcode opcode;
  std::string payload;
};

/// A frame as the server sends it: whole, and never masked.
std::string ServerFrame(Opcode opcode, std::string_view payload);

/// A frame as a client sends it, masked with the mask: the tests', and
/// anything else that talks to the gateway as a browser would.
std::string ClientFrame(Opcode opcode, std::string_view payload, uint32_t mask,
                        bool fin = true);

/// Reads a client's frames, which each have to be masked, and puts each
/// message together from its fragments. Control frames (ping, pong and close)
/// come out as they arrive, between a message's fragments too.
class FrameDecoder {
 public:
  /// A message larger than this, all its fragments together, is refused.
  explicit FrameDecoder(std::size_t max_message) : max_(max_message) {}

  void Append(std::string_view bytes);

  /// The next message, or nothing until more bytes come. After a frame that
  /// breaks the protocol, returns nothing more, and status() says why.
  std::optional<Message> Next();

  const absl::Status& status() const { return status_; }

 private:
  std::optional<Message> Fail(std::string_view why);

  std::size_t max_;
  std::string buffer_;
  std::optional<Opcode> fragmented_;
  std::string fragments_;
  absl::Status status_;
};

}  // namespace nanommo

#endif  // NANOMMO_GATEWAY_WEBSOCKET_H_
