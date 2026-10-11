#include "server/accounts.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"

namespace nanommo {

absl::Status CheckUsername(std::string_view username) {
  if (username.size() < 3 || username.size() > 16) {
    return absl::InvalidArgumentError("Names are 3 to 16 characters long.");
  }
  const auto allowed = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };
  if (!std::all_of(username.begin(), username.end(), allowed)) {
    return absl::InvalidArgumentError(
        "Names are letters, digits and underscores.");
  }
  return absl::OkStatus();
}

absl::Status CheckPassword(std::string_view password) {
  if (password.size() < 8) {
    return absl::InvalidArgumentError(
        "Passwords are at least 8 characters long.");
  }
  if (password.size() > 128) {
    return absl::InvalidArgumentError(
        "Passwords are at most 128 characters long.");
  }
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> Accounts::Create(const std::string& username,
                                          const std::string& password_hash) {
  // The unique key on the name decides, with an AlreadyExists error. Checking
  // first and inserting after would let two people take one name in between.
  const absl::StatusOr<Result> result = database_.Run(
      "INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
      {username, password_hash});
  if (!result.ok()) {
    return result.status();
  }
  return static_cast<uint32_t>(result->insert_id);
}

absl::StatusOr<Account> Accounts::Find(const std::string& username) {
  const absl::StatusOr<Result> result = database_.Run(
      "SELECT id, password_hash FROM accounts WHERE username = ?", {username});
  if (!result.ok()) {
    return result.status();
  }
  if (result->rows.empty()) {
    return absl::NotFoundError("no such account");
  }
  const std::vector<std::string>& row = result->rows.front();
  Account account{.password_hash = row[1]};
  if (!absl::SimpleAtoi(row[0], &account.id)) {
    return absl::InternalError("an account id that isn't a number");
  }
  return account;
}

}  // namespace nanommo
