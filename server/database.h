#ifndef NANOMMO_SERVER_DATABASE_H_
#define NANOMMO_SERVER_DATABASE_H_

#include <mysql.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace nanommo {

/// Closes a statement that's been prepared, as a unique_ptr goes.
struct CloseStatement {
  void operator()(MYSQL_STMT* statement) const { mysql_stmt_close(statement); }
};
using Statement = std::unique_ptr<MYSQL_STMT, CloseStatement>;

struct Result {
  std::vector<std::vector<std::string>> rows;
  uint64_t insert_id = 0;
  /// How many rows an INSERT, UPDATE or DELETE changed.
  uint64_t affected_rows = 0;
};

/// How long MySQL has to answer, and what happens when it doesn't. A world's
/// tick waits for the reads that load a player who joins, so a MySQL that
/// hangs would hang the world.
struct MysqlTimeouts {
  /// For connecting.
  std::chrono::seconds connect{1};
  /// For each read and each write on the connection, longer than the two
  /// seconds a statement waits for a lock, which fails by itself. MySQL's
  /// client counts them in seconds.
  std::chrono::seconds answer{3};
  /// After MySQL didn't answer in time, or couldn't be reached, each call
  /// fails at once for this long, without asking it.
  std::chrono::milliseconds fail_fast{5000};
};

class Database {
 public:
  /// Connects to MySQL, at the host, which can name a port as host:port, or
  /// returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Database>> Create(
      const std::string& host, const std::string& user,
      const std::string& password, const std::string& name,
      MysqlTimeouts timeouts = {});
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  /// The most reads a connection keeps prepared.
  static constexpr std::size_t kMostReads = 64;

  /// Values go apart from the SQL, one for each ?, so they stay values. A
  /// duplicate key comes back as an AlreadyExists error, a lost connection as
  /// Unavailable, and a lock waited for too long, or a deadlock, as Aborted.
  /// A SELECT is prepared the first time it runs, and kept, so that it runs
  /// in one round trip after that, not two. A statement MySQL doesn't answer
  /// in time fails as Unavailable, and so does each after it, at once, until
  /// the timeouts' fail_fast is over.
  absl::StatusOr<Result> Run(std::string_view sql,
                             const std::vector<std::string>& values = {});

  /// Runs the statements as one transaction, so that either all of them
  /// happen, or none does: none if one of them fails, or if the connection
  /// goes. A lost connection isn't tried again inside a transaction, as what
  /// ran before it on that connection is gone.
  absl::Status Transaction(const std::function<absl::Status()>& statements);

 private:
  Database(std::string host, std::string user, std::string password,
           std::string name, MysqlTimeouts timeouts)
      : host_(std::move(host)),
        user_(std::move(user)),
        password_(std::move(password)),
        name_(std::move(name)),
        timeouts_(timeouts) {}

  absl::Status Connect();
  absl::StatusOr<Result> RunOnce(std::string_view sql,
                                 const std::vector<std::string>& values,
                                 std::chrono::steady_clock::time_point start);
  /// Whether to ask MySQL at all: not while failing fast.
  absl::Status Ready() const;
  /// Whether a call that began then and failed as Unavailable waited out its
  /// timeout, rather than finding the connection gone.
  bool TimedOut(std::chrono::steady_clock::time_point start) const;
  /// Fails each call at once for the timeouts' fail_fast, after a call that
  /// failed so.
  void Notice(const absl::Status& status,
              std::chrono::steady_clock::time_point start);
  absl::StatusOr<Result> Execute(MYSQL_STMT* statement,
                                 const std::vector<std::string>& values);

  std::string host_;
  std::string user_;
  std::string password_;
  std::string name_;
  MysqlTimeouts timeouts_;
  std::chrono::steady_clock::time_point failing_until_;
  MYSQL* mysql_ = nullptr;
  bool in_transaction_ = false;
  /// The reads prepared on this connection, by their SQL.
  std::map<std::string, Statement, std::less<>> reads_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_DATABASE_H_
