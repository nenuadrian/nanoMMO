#ifndef NANOMMO_SERVER_ACCOUNTS_H_
#define NANOMMO_SERVER_ACCOUNTS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "server/database.h"

namespace nanommo {

/// An InvalidArgument error says what's wrong, in words for the player.
absl::Status CheckUsername(std::string_view username);

absl::Status CheckPassword(std::string_view password);

struct Account {
  uint32_t id = 0;
  std::string password_hash;
};

class Accounts {
 public:
  explicit Accounts(Database& database) : database_(database) {}

  /// Returns an AlreadyExists error if the name is taken.
  absl::StatusOr<uint32_t> Create(const std::string& username,
                                  const std::string& password_hash);

  /// Returns a NotFound error if there's no account by that name.
  absl::StatusOr<Account> Find(const std::string& username);

 private:
  Database& database_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_ACCOUNTS_H_
