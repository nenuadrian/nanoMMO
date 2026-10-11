#ifndef NANOMMO_SERVER_SETTINGS_H_
#define NANOMMO_SERVER_SETTINGS_H_

#include <cstdlib>
#include <string>

namespace nanommo {

inline std::string Setting(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr ? value : fallback;
}

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SETTINGS_H_
