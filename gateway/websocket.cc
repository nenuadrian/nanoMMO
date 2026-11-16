#include "gateway/websocket.h"

#include <openssl/evp.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace nanommo {
namespace {

// RFC 6455's, the same for every server.
constexpr std::string_view kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// A control frame's payload fits in its first length byte.
constexpr std::size_t kMaxControl = 125;

// Whether a comma-separated header value has the token, in any case.
bool HasToken(std::string_view value, std::string_view token) {
  for (const std::string_view part : absl::StrSplit(value, ',')) {
    if (absl::EqualsIgnoreCase(absl::StripAsciiWhitespace(part), token)) {
      return true;
    }
  }
  return false;
}

std::string_view Reason(int status) {
  switch (status) {
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 502:
      return "Bad Gateway";
    default:
      return "Error";
  }
}

// The length, in the shortest of the three ways the protocol has, after the
// first byte; masked or not in the length byte's top bit.
void AppendLength(std::string& frame, std::size_t size, bool masked) {
  const auto mask_bit = static_cast<unsigned char>(masked ? 0x80 : 0);
  if (size <= kMaxControl) {
    frame.push_back(static_cast<char>(mask_bit | size));
  } else if (size <= 0xFFFF) {
    frame.push_back(static_cast<char>(mask_bit | 126));
    frame.push_back(static_cast<char>((size >> 8) & 0xFF));
    frame.push_back(static_cast<char>(size & 0xFF));
  } else {
    frame.push_back(static_cast<char>(mask_bit | 127));
    for (int shift = 56; shift >= 0; shift -= 8) {
      frame.push_back(static_cast<char>((size >> shift) & 0xFF));
    }
  }
}

}  // namespace

absl::StatusOr<Handshake> ParseHandshake(std::string_view request) {
  std::vector<std::string_view> lines = absl::StrSplit(request, "\r\n");
  // GET <path> HTTP/1.1, then a header a line.
  const std::vector<std::string_view> first = absl::StrSplit(lines[0], ' ');
  if (first.size() != 3 || first[0] != "GET" ||
      !absl::StartsWith(first[2], "HTTP/1.1") || first[1].empty() ||
      first[1][0] != '/') {
    return absl::InvalidArgumentError("not an HTTP/1.1 GET");
  }
  bool upgrade = false;
  bool connection = false;
  bool version = false;
  Handshake handshake{.path = std::string(first[1]), .key = ""};
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const std::size_t colon = lines[i].find(':');
    if (colon == std::string_view::npos) {
      continue;
    }
    const std::string_view name =
        absl::StripAsciiWhitespace(lines[i].substr(0, colon));
    const std::string_view value =
        absl::StripAsciiWhitespace(lines[i].substr(colon + 1));
    if (absl::EqualsIgnoreCase(name, "Upgrade")) {
      upgrade = HasToken(value, "websocket");
    } else if (absl::EqualsIgnoreCase(name, "Connection")) {
      connection = HasToken(value, "upgrade");
    } else if (absl::EqualsIgnoreCase(name, "Sec-WebSocket-Version")) {
      version = value == "13";
    } else if (absl::EqualsIgnoreCase(name, "Sec-WebSocket-Key")) {
      handshake.key = std::string(value);
    }
  }
  if (!upgrade || !connection) {
    return absl::InvalidArgumentError("not an upgrade to websocket");
  }
  if (!version) {
    return absl::InvalidArgumentError("not WebSocket version 13");
  }
  // The key is 16 random bytes in base 64.
  std::string bytes;
  if (!absl::Base64Unescape(handshake.key, &bytes) || bytes.size() != 16) {
    return absl::InvalidArgumentError("no Sec-WebSocket-Key of 16 bytes");
  }
  return handshake;
}

std::string AcceptKey(std::string_view key) {
  const std::string input = absl::StrCat(key, kGuid);
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  EVP_Digest(input.data(), input.size(), digest.data(), &size, EVP_sha1(),
             nullptr);
  return absl::Base64Escape(std::string(digest.begin(), digest.begin() + size));
}

std::string Accept(std::string_view key) {
  return absl::StrCat(
      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
      "Connection: Upgrade\r\nSec-WebSocket-Accept: ",
      AcceptKey(key), "\r\n\r\n");
}

std::string Refuse(int status, std::string_view why) {
  return absl::StrCat("HTTP/1.1 ", status, " ", Reason(status),
                      "\r\nContent-Type: text/plain\r\nContent-Length: ",
                      why.size(), "\r\nConnection: close\r\n\r\n", why);
}

std::string ServerFrame(Opcode opcode, std::string_view payload) {
  std::string frame;
  frame.reserve(payload.size() + 10);
  frame.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(opcode)));
  AppendLength(frame, payload.size(), /*masked=*/false);
  frame.append(payload);
  return frame;
}

std::string ClientFrame(Opcode opcode, std::string_view payload, uint32_t mask,
                        bool fin) {
  std::string frame;
  frame.push_back(
      static_cast<char>((fin ? 0x80 : 0) | static_cast<uint8_t>(opcode)));
  AppendLength(frame, payload.size(), /*masked=*/true);
  const std::array<char, 4> key = {
      static_cast<char>(mask >> 24), static_cast<char>((mask >> 16) & 0xFF),
      static_cast<char>((mask >> 8) & 0xFF), static_cast<char>(mask & 0xFF)};
  frame.append(key.data(), key.size());
  for (std::size_t i = 0; i < payload.size(); ++i) {
    frame.push_back(static_cast<char>(payload[i] ^ key[i % 4]));
  }
  return frame;
}

void FrameDecoder::Append(std::string_view bytes) {
  if (status_.ok()) {
    buffer_.append(bytes);
  }
}

std::optional<Message> FrameDecoder::Fail(std::string_view why) {
  status_ = absl::InvalidArgumentError(why);
  buffer_.clear();
  fragments_.clear();
  return std::nullopt;
}

std::optional<Message> FrameDecoder::Next() {
  while (status_.ok()) {
    if (buffer_.size() < 2) {
      return std::nullopt;
    }
    const auto byte = [&](std::size_t i) {
      return static_cast<uint8_t>(buffer_[i]);
    };
    const bool fin = (byte(0) & 0x80) != 0;
    if ((byte(0) & 0x70) != 0) {
      return Fail("a frame uses bits no extension was agreed for");
    }
    const auto opcode = static_cast<Opcode>(byte(0) & 0x0F);
    const bool control = (byte(0) & 0x08) != 0;
    switch (opcode) {
      case Opcode::kContinuation:
      case Opcode::kText:
      case Opcode::kBinary:
      case Opcode::kClose:
      case Opcode::kPing:
      case Opcode::kPong:
        break;
      default:
        return Fail("a frame with an opcode the protocol doesn't have");
    }
    if ((byte(1) & 0x80) == 0) {
      return Fail("a client's frame has to be masked");
    }
    std::size_t header = 2;
    uint64_t size = byte(1) & 0x7F;
    if (size == 126) {
      header = 4;
      if (buffer_.size() < header) {
        return std::nullopt;
      }
      size = (uint64_t{byte(2)} << 8) | byte(3);
    } else if (size == 127) {
      header = 10;
      if (buffer_.size() < header) {
        return std::nullopt;
      }
      size = 0;
      for (std::size_t i = 2; i < 10; ++i) {
        size = (size << 8) | byte(i);
      }
    }
    if (control && (!fin || size > kMaxControl)) {
      return Fail("a control frame has to be whole, and 125 bytes at most");
    }
    // Refused before it's all here, so a client can't make us hold more.
    if (!control && fragments_.size() + size > max_) {
      return Fail(absl::StrCat("a message over ", max_, " bytes"));
    }
    if (buffer_.size() < header + 4 + size) {
      return std::nullopt;
    }
    const std::string_view mask(buffer_.data() + header, 4);
    std::string payload(buffer_, header + 4, size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
      payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
    }
    buffer_.erase(0, header + 4 + size);
    if (control) {
      return Message{.opcode = opcode, .payload = std::move(payload)};
    }
    if (opcode == Opcode::kContinuation) {
      if (!fragmented_) {
        return Fail("a continuation of no message");
      }
    } else if (fragmented_) {
      return Fail("a new message before the last one's end");
    } else {
      fragmented_ = opcode;
    }
    fragments_.append(payload);
    if (fin) {
      Message message{.opcode = *fragmented_, .payload = std::move(fragments_)};
      fragments_.clear();
      fragmented_.reset();
      return message;
    }
  }
  return std::nullopt;
}

}  // namespace nanommo
