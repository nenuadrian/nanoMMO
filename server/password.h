#ifndef NANOMMO_SERVER_PASSWORD_H_
#define NANOMMO_SERVER_PASSWORD_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"

namespace nanommo {

absl::StatusOr<std::string> HashPassword(std::string_view password);

bool VerifyPassword(const std::string& hash, std::string_view password);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_PASSWORD_H_
