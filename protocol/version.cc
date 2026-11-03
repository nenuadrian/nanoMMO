#include "protocol/version.h"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

namespace nanommo {

namespace {

// MAJOR.MINOR.PATCH as three numbers, or nothing if it isn't that.
std::optional<std::array<int, 3>> Parse(std::string_view version) {
  const std::vector<std::string_view> parts = absl::StrSplit(version, '.');
  if (parts.size() != 3) {
    return std::nullopt;
  }
  std::array<int, 3> numbers;
  for (int i = 0; i < 3; ++i) {
    if (!absl::SimpleAtoi(parts[i], &numbers[i]) || numbers[i] < 0) {
      return std::nullopt;
    }
  }
  return numbers;
}

}  // namespace

bool Compatible(std::string_view client, std::string_view server) {
  const std::optional<std::array<int, 3>> a = Parse(client);
  const std::optional<std::array<int, 3>> b = Parse(server);
  return a && b && (*a)[0] == (*b)[0] && (*a)[1] == (*b)[1];
}

}  // namespace nanommo
