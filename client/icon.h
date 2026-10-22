#ifndef NANOMMO_CLIENT_ICON_H_
#define NANOMMO_CLIENT_ICON_H_

#include <cstdint>
#include <string>

namespace nanommo {

/// A Font Awesome icon, by its code point, as text for ImGui. The icons are in
/// Unicode's private use area, between U+E000 and U+F8FF, which UTF-8 writes
/// in three bytes.
inline std::string Icon(uint32_t code) {
  return {static_cast<char>(0xE0 | (code >> 12)),
          static_cast<char>(0x80 | ((code >> 6) & 0x3F)),
          static_cast<char>(0x80 | (code & 0x3F))};
}

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ICON_H_
