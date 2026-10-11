#include "protocol/framing.h"

#include <cstdint>
#include <string>

namespace nanommo {

std::string Header(std::size_t size) {
  const auto value = static_cast<uint32_t>(size);
  return {static_cast<char>(value >> 24), static_cast<char>(value >> 16),
          static_cast<char>(value >> 8), static_cast<char>(value)};
}

std::string Frame(const google::protobuf::MessageLite& message) {
  const std::string body = message.SerializeAsString();
  return Header(body.size()) + body;
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
