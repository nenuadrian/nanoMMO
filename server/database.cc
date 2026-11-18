#include "server/database.h"

#include <errmsg.h>
#include <mysqld_error.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "server/tracing.h"

namespace nanommo {

namespace {

// Room for the longest value we read back, a password hash.
constexpr int kValueSize = 256;

// The C API's own type for a length, whatever it is on this system.
using Length = std::remove_pointer_t<decltype(MYSQL_BIND::length)>;

struct Value {
  std::array<char, kValueSize> bytes;
  Length length = 0;
  bool null = false;
};

absl::Status Error(unsigned int code, const char* message) {
  if (code == ER_DUP_ENTRY) {
    return absl::AlreadyExistsError(message);
  }
  // Another transaction held what this one needed for too long, or the two
  // waited for each other: it's worth trying again.
  if (code == ER_LOCK_WAIT_TIMEOUT || code == ER_LOCK_DEADLOCK) {
    return absl::AbortedError(absl::StrCat("MySQL gave up: ", message));
  }
  if (code == CR_SERVER_GONE_ERROR || code == CR_SERVER_LOST ||
      code == ER_CLIENT_INTERACTION_TIMEOUT) {
    return absl::UnavailableError(
        absl::StrCat("lost the connection to MySQL: ", message));
  }
  return absl::InternalError(absl::StrCat("MySQL error ", code, ": ", message));
}

absl::Status StatementError(MYSQL_STMT* statement) {
  return Error(mysql_stmt_errno(statement), mysql_stmt_error(statement));
}

// A statement, as a span under the request it's run for, named by what it does
// and to which table, such as SELECT players. Its SQL has a ? for each value,
// so the values stay out of it.
Span StartStatement(std::string_view sql) {
  if (!InTrace()) {
    return Span();
  }
  std::string_view operation;
  std::string_view table;
  bool table_next = false;
  for (const std::string_view word :
       absl::StrSplit(sql, absl::ByAnyChar(" (),\n"), absl::SkipEmpty())) {
    if (operation.empty()) {
      operation = word;
      table_next = operation == "UPDATE";
    } else if (table_next && word != "SELECT") {
      table = word;
      break;
    } else {
      table_next = word == "FROM" || word == "INTO";
    }
  }
  Span span = StartChildSpan(absl::StrCat("mysql ", operation, " ", table),
                             SpanKind::kClient);
  span.SetAttribute("db.system", "mysql");
  span.SetAttribute("db.operation", operation);
  span.SetAttribute("db.sql.table", table);
  // The saver's statements run to thousands of ?, so a few hundred characters
  // say which one it is.
  span.SetAttribute("db.statement", sql.substr(0, 300));
  return span;
}

absl::StatusOr<Statement> Prepare(MYSQL* mysql, std::string_view sql) {
  Statement statement(mysql_stmt_init(mysql));
  if (!statement) {
    return Error(mysql_errno(mysql), mysql_error(mysql));
  }
  if (mysql_stmt_prepare(statement.get(), sql.data(), sql.size()) != 0) {
    return StatementError(statement.get());
  }
  return statement;
}

}  // namespace

absl::StatusOr<std::unique_ptr<Database>> Database::Create(
    const std::string& host, const std::string& user,
    const std::string& password, const std::string& name,
    MysqlTimeouts timeouts) {
  std::unique_ptr<Database> database =
      absl::WrapUnique(new Database(host, user, password, name, timeouts));
  const absl::Status connected = database->Connect();
  if (!connected.ok()) {
    return connected;
  }
  return database;
}

absl::Status Database::Connect() {
  MYSQL* mysql = mysql_init(nullptr);
  if (mysql == nullptr) {
    return absl::ResourceExhaustedError("out of memory");
  }
  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");
  const auto connect = static_cast<unsigned int>(timeouts_.connect.count());
  const auto answer = static_cast<unsigned int>(timeouts_.answer.count());
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connect);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &answer);
  mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &answer);
  // A host can name its port, as host:port, or it's MySQL's.
  const std::vector<std::string> host = absl::StrSplit(host_, ':');
  unsigned int port = 0;
  if (host.size() == 2 && !absl::SimpleAtoi(host[1], &port)) {
    mysql_close(mysql);
    return absl::InvalidArgumentError(
        absl::StrCat("a MySQL host's port that isn't a number: ", host_));
  }
  if (mysql_real_connect(mysql, host[0].c_str(), user_.c_str(),
                         password_.c_str(), name_.c_str(), port, nullptr,
                         0) == nullptr) {
    const absl::Status status = absl::UnavailableError(
        absl::StrCat("couldn't connect to MySQL: ", mysql_error(mysql)));
    mysql_close(mysql);
    failing_until_ = std::chrono::steady_clock::now() + timeouts_.fail_fast;
    return status;
  }
  // A world's tick waits for what it asks of MySQL, so a statement waits two
  // seconds at most for a lock another transaction holds, not MySQL's fifty.
  if (mysql_query(mysql, "SET SESSION innodb_lock_wait_timeout = 2") != 0) {
    const absl::Status status = Error(mysql_errno(mysql), mysql_error(mysql));
    mysql_close(mysql);
    return status;
  }
  // The old connection goes once the new one works, and the statements
  // prepared on it with it.
  reads_.clear();
  mysql_close(mysql_);
  mysql_ = mysql;
  return absl::OkStatus();
}

Database::~Database() {
  reads_.clear();
  mysql_close(mysql_);
}

absl::Status Database::Ready() const {
  if (std::chrono::steady_clock::now() < failing_until_) {
    return absl::UnavailableError(
        "MySQL didn't answer a moment ago, so it isn't asked yet");
  }
  return absl::OkStatus();
}

bool Database::TimedOut(std::chrono::steady_clock::time_point start) const {
  return std::chrono::steady_clock::now() - start >= timeouts_.answer;
}

void Database::Notice(const absl::Status& status,
                      std::chrono::steady_clock::time_point start) {
  if (absl::IsUnavailable(status) && TimedOut(start)) {
    failing_until_ = std::chrono::steady_clock::now() + timeouts_.fail_fast;
  }
}

absl::StatusOr<Result> Database::Run(std::string_view sql,
                                     const std::vector<std::string>& values) {
  const Span span = StartStatement(sql);
  if (const absl::Status ready = Ready(); !ready.ok()) {
    return ready;
  }
  const auto start = std::chrono::steady_clock::now();
  absl::StatusOr<Result> result = RunOnce(sql, values, start);
  Notice(result.status(), start);
  return result;
}

absl::StatusOr<Result> Database::RunOnce(
    std::string_view sql, const std::vector<std::string>& values,
    std::chrono::steady_clock::time_point start) {
  if (const auto kept = reads_.find(sql); kept != reads_.end()) {
    absl::StatusOr<Result> result = Execute(kept->second.get(), values);
    if (result.ok()) {
      return result;
    }
    // It's prepared again next time.
    reads_.erase(kept);
    // A connection that's gone stale since it was prepared fails as it runs.
    // A read changed nothing, so it runs again, on a new connection, unless
    // we're in a transaction, which went with the old one. One that waited
    // out its timeout found MySQL not answering, and asking again would wait
    // as long again.
    if (!absl::IsUnavailable(result.status()) || in_transaction_ ||
        TimedOut(start)) {
      return result;
    }
  }
  absl::StatusOr<Statement> prepared = Prepare(mysql_, sql);
  // MySQL drops a connection that sits idle for eight hours, and a restart
  // drops them all. Nothing has run yet, so we connect again and retry,
  // unless we're in a transaction, which went with the connection, or MySQL
  // didn't answer in time.
  if (absl::IsUnavailable(prepared.status()) && !in_transaction_ &&
      !TimedOut(start)) {
    const absl::Status connected = Connect();
    if (!connected.ok()) {
      return connected;
    }
    prepared = Prepare(mysql_, sql);
  }
  if (!prepared.ok()) {
    return prepared.status();
  }
  Statement statement = *std::move(prepared);
  absl::StatusOr<Result> result = Execute(statement.get(), values);
  if (result.ok() && sql.starts_with("SELECT") && reads_.size() < kMostReads) {
    reads_.emplace(sql, std::move(statement));
  }
  return result;
}

absl::StatusOr<Result> Database::Execute(
    MYSQL_STMT* statement, const std::vector<std::string>& values) {
  if (mysql_stmt_param_count(statement) != values.size()) {
    return absl::InvalidArgumentError(
        "the statement needs a different number of values");
  }

  // Each value goes in as text, and MySQL converts it to its column's type.
  std::vector<MYSQL_BIND> params(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    params[i].buffer_type = MYSQL_TYPE_STRING;
    params[i].buffer = const_cast<char*>(values[i].data());
    params[i].buffer_length = values[i].size();
  }
  if ((!params.empty() &&
       mysql_stmt_bind_param(statement, params.data()) != 0) ||
      mysql_stmt_execute(statement) != 0) {
    return StatementError(statement);
  }

  Result result;
  result.insert_id = mysql_stmt_insert_id(statement);
  result.affected_rows = mysql_stmt_affected_rows(statement);
  const unsigned int columns = mysql_stmt_field_count(statement);
  if (columns == 0) {
    return result;
  }

  std::vector<Value> row(columns);
  std::vector<MYSQL_BIND> bound(columns);
  for (unsigned int i = 0; i < columns; ++i) {
    bound[i].buffer_type = MYSQL_TYPE_STRING;
    bound[i].buffer = row[i].bytes.data();
    bound[i].buffer_length = kValueSize;
    bound[i].length = &row[i].length;
    bound[i].is_null = &row[i].null;
  }
  if (mysql_stmt_bind_result(statement, bound.data()) != 0) {
    return StatementError(statement);
  }
  int status = 0;
  while ((status = mysql_stmt_fetch(statement)) == 0) {
    std::vector<std::string>& copy = result.rows.emplace_back();
    for (const Value& value : row) {
      copy.emplace_back(value.bytes.data(), value.null ? 0 : value.length);
    }
  }
  if (status == MYSQL_DATA_TRUNCATED) {
    return absl::OutOfRangeError(
        absl::StrCat("a value didn't fit in ", kValueSize, " bytes"));
  }
  if (status != MYSQL_NO_DATA) {
    return StatementError(statement);
  }
  return result;
}

absl::Status Database::Transaction(
    const std::function<absl::Status()>& statements) {
  // Its statements' spans go under it, and the commit's time is its own.
  const Span span = StartChildSpan("mysql transaction", SpanKind::kInternal);
  if (const absl::Status ready = Ready(); !ready.ok()) {
    return ready;
  }
  const auto start = std::chrono::steady_clock::now();
  // Turning autocommit off is the first thing to reach MySQL, so a connection
  // that's gone stale fails there, before anything has run, and we connect
  // again.
  if (mysql_autocommit(mysql_, false) != 0) {
    const absl::Status connected =
        TimedOut(start) ? absl::UnavailableError("MySQL didn't answer in time")
                        : Connect();
    if (!connected.ok()) {
      Notice(connected, start);
      return connected;
    }
    if (mysql_autocommit(mysql_, false) != 0) {
      return Error(mysql_errno(mysql_), mysql_error(mysql_));
    }
  }
  in_transaction_ = true;
  absl::Status status = statements();
  in_transaction_ = false;
  if (status.ok() && mysql_commit(mysql_) != 0) {
    status = Error(mysql_errno(mysql_), mysql_error(mysql_));
  }
  if (!status.ok()) {
    mysql_rollback(mysql_);
  }
  mysql_autocommit(mysql_, true);
  Notice(status, start);
  return status;
}

}  // namespace nanommo
