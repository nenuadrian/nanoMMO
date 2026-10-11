#include "protocol/framing.h"

#include <cstdint>
#include <string>

namespace nanommo {

namespace {

constexpr std::size_t kHeaderSize = 4;

}

std::string Frame(const google::protobuf::MessageLite& message) {
  const std::string body = message.SerializeAsString();
  const auto size = static_cast<uint32_t>(body.size());

  std::string bytes;
  bytes.reserve(kHeaderSize + body.size());
  bytes.push_back(static_cast<char>(size >> 24));
  bytes.push_back(static_cast<char>(size >> 16));
  bytes.push_back(static_cast<char>(size >> 8));
  bytes.push_back(static_cast<char>(size));
  bytes += body;
  return bytes;
}

void FrameReader::Append(std::string_view bytes) { buffer_.append(bytes); }

std::optional<std::string> FrameReader::Next() {
  if (buffer_.size() < kHeaderSize) {
    return std::nullopt;
  }
  uint32_t size = 0;
  for (std::size_t i = 0; i < kHeaderSize; ++i) {
    size = (size << 8) | static_cast<uint8_t>(buffer_[i]);
  }
  if (size > kMaxMessageSize) {
    status_ = absl::InvalidArgumentError("message of " + std::to_string(size) +
                                         " bytes is too large");
    return std::nullopt;
  }
  if (buffer_.size() < kHeaderSize + size) {
    return std::nullopt;
  }

  std::string message = buffer_.substr(kHeaderSize, size);
  buffer_.erase(0, kHeaderSize + size);
  return message;
}

}  // namespace nanommo
