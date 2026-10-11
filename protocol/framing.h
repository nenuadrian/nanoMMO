#ifndef NANOMMO_PROTOCOL_FRAMING_H_
#define NANOMMO_PROTOCOL_FRAMING_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "google/protobuf/message_lite.h"

namespace nanommo {

/// Anything bigger is garbage. Without a cap, we might buffer up to 4 GB.
constexpr std::size_t kMaxMessageSize = 64 * 1024;

/// The size goes in front, as four bytes in network byte order.
std::string Frame(const google::protobuf::MessageLite& message);

class FrameReader {
 public:
  void Append(std::string_view bytes);

  /// After a message that's too large, returns nothing more, and status()
  /// says why.
  std::optional<std::string> Next();

  const absl::Status& status() const { return status_; }

 private:
  std::string buffer_;
  absl::Status status_;
};

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_FRAMING_H_
