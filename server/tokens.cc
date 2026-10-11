#include "server/tokens.h"

#include <sodium.h>

#include <array>
#include <string>

#include "absl/log/absl_check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

constexpr char kPrefix[] = "token:";

}  // namespace

Tokens::Tokens(Redis& redis) : redis_(redis) {
  ABSL_CHECK(sodium_init() >= 0) << "couldn't initialise libsodium";
}

absl::StatusOr<std::string> Tokens::Issue(uint32_t account_id) {
  std::array<unsigned char, 32> bytes;
  randombytes_buf(bytes.data(), bytes.size());
  std::array<char, 2 * bytes.size() + 1> hex;
  sodium_bin2hex(hex.data(), hex.size(), bytes.data(), bytes.size());
  const std::string token = hex.data();

  const absl::StatusOr<std::string> stored = redis_.Command(
      {"SET", absl::StrCat(kPrefix, token), absl::StrCat(account_id), "EX",
       absl::StrCat(kLifetime.count())});
  if (!stored.ok()) {
    return stored.status();
  }
  return token;
}

absl::StatusOr<uint32_t> Tokens::Redeem(const std::string& token) {
  // GETDEL reads and deletes at once, so two clients can't use one token.
  const absl::StatusOr<std::string> account =
      redis_.Command({"GETDEL", absl::StrCat(kPrefix, token)});
  if (!account.ok()) {
    return account.status();
  }
  uint32_t id = 0;
  if (!absl::SimpleAtoi(*account, &id)) {
    return absl::InternalError("a token for an account id that isn't a number");
  }
  return id;
}

}  // namespace nanommo
