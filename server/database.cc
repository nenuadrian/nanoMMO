#include "server/database.h"

#include <errmsg.h>
#include <mysqld_error.h>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"

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

struct CloseStatement {
  void operator()(MYSQL_STMT* statement) const { mysql_stmt_close(statement); }
};
using Statement = std::unique_ptr<MYSQL_STMT, CloseStatement>;

absl::Status Error(unsigned int code, const char* message) {
  if (code == ER_DUP_ENTRY) {
    return absl::AlreadyExistsError(message);
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
    const std::string& password, const std::string& name) {
  std::unique_ptr<Database> database =
      absl::WrapUnique(new Database(host, user, password, name));
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
  if (mysql_real_connect(mysql, host_.c_str(), user_.c_str(), password_.c_str(),
                         name_.c_str(), 0, nullptr, 0) == nullptr) {
    const absl::Status status = absl::UnavailableError(
        absl::StrCat("couldn't connect to MySQL: ", mysql_error(mysql)));
    mysql_close(mysql);
    return status;
  }
  // The old connection goes once the new one works.
  mysql_close(mysql_);
  mysql_ = mysql;
  return absl::OkStatus();
}

Database::~Database() { mysql_close(mysql_); }

absl::StatusOr<Result> Database::Run(std::string_view sql,
                                     const std::vector<std::string>& values) {
  absl::StatusOr<Statement> prepared = Prepare(mysql_, sql);
  // MySQL drops a connection that sits idle for eight hours, and a restart
  // drops them all. Nothing has run yet, so we connect again and retry,
  // unless we're in a transaction, which went with the connection.
  if (absl::IsUnavailable(prepared.status()) && !in_transaction_) {
    const absl::Status connected = Connect();
    if (!connected.ok()) {
      return connected;
    }
    prepared = Prepare(mysql_, sql);
  }
  if (!prepared.ok()) {
    return prepared.status();
  }
  const Statement statement = *std::move(prepared);
  if (mysql_stmt_param_count(statement.get()) != values.size()) {
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
       mysql_stmt_bind_param(statement.get(), params.data()) != 0) ||
      mysql_stmt_execute(statement.get()) != 0) {
    return StatementError(statement.get());
  }

  Result result;
  result.insert_id = mysql_stmt_insert_id(statement.get());
  const unsigned int columns = mysql_stmt_field_count(statement.get());
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
  if (mysql_stmt_bind_result(statement.get(), bound.data()) != 0) {
    return StatementError(statement.get());
  }
  int status = 0;
  while ((status = mysql_stmt_fetch(statement.get())) == 0) {
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
    return StatementError(statement.get());
  }
  return result;
}

absl::Status Database::Transaction(
    const std::function<absl::Status()>& statements) {
  // Turning autocommit off is the first thing to reach MySQL, so a connection
  // that's gone stale fails there, before anything has run, and we connect
  // again.
  if (mysql_autocommit(mysql_, false) != 0) {
    const absl::Status connected = Connect();
    if (!connected.ok()) {
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
  return status;
}

}  // namespace nanommo
