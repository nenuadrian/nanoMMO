#ifndef NANOMMO_PROTOCOL_SETTINGS_H_
#define NANOMMO_PROTOCOL_SETTINGS_H_

#include <cstdlib>
#include <string>

namespace nanommo {

inline std::string Setting(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr ? value : fallback;
}

/// A file or a folder a program reads: the setting called name, if it's set,
/// or else path in the repository the program was built from, so it runs from
/// any folder. Where the repository hasn't got it, as in a Docker image, which
/// leaves the repository behind, path is from the folder it runs in. A path in
/// neither comes back from the repository, where an error should send you.
std::string FileSetting(const char* name, const std::string& path);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_SETTINGS_H_
