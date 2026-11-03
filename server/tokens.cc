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
constexpr char kSessionPrefix[] = "session:";

// 32 random bytes, in hex.
std::string RandomHex() {
  std::array<unsigned char, 32> bytes;
  randombytes_buf(bytes.data(), bytes.size());
  std::array<char, 2 * bytes.size() + 1> hex;
  sodium_bin2hex(hex.data(), hex.size(), bytes.data(), bytes.size());
  return hex.data();
}

}  // namespace

Tokens::Tokens(Redis& redis) : redis_(redis) {
  ABSL_CHECK(sodium_init() >= 0) << "couldn't initialise libsodium";
}

absl::StatusOr<std::string> Tokens::Issue(uint32_t player_id) {
  std::string token = RandomHex();
  const absl::StatusOr<std::string> stored = redis_.Command(
      {"SET", absl::StrCat(kPrefix, token), absl::StrCat(player_id), "EX",
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
    return absl::InternalError("a token for an id that isn't a number");
  }
  return id;
}

absl::StatusOr<std::string> Tokens::Open(uint32_t account_id) {
  std::string session = RandomHex();
  const absl::StatusOr<std::string> stored = redis_.Command(
      {"SET", absl::StrCat(kSessionPrefix, session), absl::StrCat(account_id),
       "EX", absl::StrCat(std::chrono::seconds(kSessionLifetime).count())});
  if (!stored.ok()) {
    return stored.status();
  }
  return session;
}

absl::StatusOr<uint32_t> Tokens::AccountOf(const std::string& session) {
  // GETEX reads it and starts its lifetime again, at once.
  const absl::StatusOr<std::string> account = redis_.Command(
      {"GETEX", absl::StrCat(kSessionPrefix, session), "EX",
       absl::StrCat(std::chrono::seconds(kSessionLifetime).count())});
  if (!account.ok()) {
    return account.status();
  }
  uint32_t id = 0;
  if (!absl::SimpleAtoi(*account, &id)) {
    return absl::InternalError("a session for an id that isn't a number");
  }
  return id;
}

}  // namespace nanommo
