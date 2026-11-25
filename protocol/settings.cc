#include "protocol/settings.h"

#include <sys/stat.h>

#include <cstdlib>
#include <string>

#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

// stat, which Windows has too, where it hasn't POSIX's access.
bool Exists(const std::string& path) {
  struct stat status;
  return stat(path.c_str(), &status) == 0;
}

}  // namespace

std::string FileSetting(const char* name, const std::string& path) {
  if (const char* value = std::getenv(name)) {
    return value;
  }
  // The build puts the repository's folder in NANOMMO_SOURCE_DIR.
  // Not const, so that returning it moves it.
  std::string repository = absl::StrCat(NANOMMO_SOURCE_DIR, "/", path);
  if (Exists(repository) || !Exists(path)) {
    return repository;
  }
  return path;
}

}  // namespace nanommo
