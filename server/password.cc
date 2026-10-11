#include "server/password.h"

#include <sodium.h>

#include <array>
#include <string>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"

namespace nanommo {

namespace {

void InitSodium() {
  ABSL_CHECK(sodium_init() >= 0) << "couldn't initialise libsodium";
}

}  // namespace

absl::StatusOr<std::string> HashPassword(std::string_view password) {
  InitSodium();
  std::array<char, crypto_pwhash_STRBYTES> hash{};
  // What libsodium suggests for logins: two passes over 64 MiB of memory.
  if (crypto_pwhash_str(hash.data(), password.data(), password.size(),
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    return absl::ResourceExhaustedError("ran out of memory hashing a password");
  }
  return std::string(hash.data());
}

bool VerifyPassword(const std::string& hash, std::string_view password) {
  InitSodium();
  return crypto_pwhash_str_verify(hash.c_str(), password.data(),
                                  password.size()) == 0;
}

}  // namespace nanommo
