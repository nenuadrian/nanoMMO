#ifndef NANOMMO_SERVER_DATABASE_H_
#define NANOMMO_SERVER_DATABASE_H_

#include <mysql.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace nanommo {

struct Result {
  std::vector<std::vector<std::string>> rows;
  uint64_t insert_id = 0;
};

class Database {
 public:
  /// Connects to MySQL, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Database>> Create(
      const std::string& host, const std::string& user,
      const std::string& password, const std::string& name);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  /// Values go apart from the SQL, one for each ?, so they stay values. A
  /// duplicate key comes back as an AlreadyExists error, and a lost
  /// connection as Unavailable.
  absl::StatusOr<Result> Run(std::string_view sql,
                             const std::vector<std::string>& values = {});

 private:
  Database(std::string host, std::string user, std::string password,
           std::string name)
      : host_(std::move(host)),
        user_(std::move(user)),
        password_(std::move(password)),
        name_(std::move(name)) {}

  absl::Status Connect();

  std::string host_;
  std::string user_;
  std::string password_;
  std::string name_;
  MYSQL* mysql_ = nullptr;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_DATABASE_H_
