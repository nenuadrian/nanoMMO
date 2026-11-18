#ifndef NANOMMO_SERVER_REDIS_H_
#define NANOMMO_SERVER_REDIS_H_

#include <hiredis.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace nanommo {

/// How long Redis has to answer, and what happens when it doesn't. A world's
/// tick waits for its saves and its leases, so a Redis that hangs would hang
/// every world.
struct RedisTimeouts {
  /// For connecting, and then for each reply.
  std::chrono::milliseconds answer{250};
  /// After Redis didn't answer in time, or couldn't be reached, each call
  /// fails at once for this long, without asking it, so that a tick pays for
  /// one timeout a second, not one each call.
  std::chrono::milliseconds fail_fast{1000};
};

class Redis {
 public:
  /// Connects to Redis, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Redis>> Create(
      const std::string& host, int port = 6379, RedisTimeouts timeouts = {});
  ~Redis();

  Redis(const Redis&) = delete;
  Redis& operator=(const Redis&) = delete;

  /// Returns the reply as text, or a NotFound error when Redis says nil. A
  /// command that loses the connection fails, and the next one reconnects.
  /// One that Redis doesn't answer in time fails as Unavailable, and so does
  /// each after it, at once, until the timeouts' fail_fast is over; its
  /// connection goes, as the reply might come later, as another's.
  absl::StatusOr<std::string> Command(const std::vector<std::string>& words);

  /// Sends all the commands before reading any reply, so they share a single
  /// round trip. Each reply comes back as a list of values, empty for nil.
  absl::StatusOr<std::vector<std::vector<std::string>>> Pipeline(
      const std::vector<std::vector<std::string>>& commands);

  /// Runs the commands as one transaction, between MULTI and EXEC, in a
  /// single round trip: Redis runs them all at once, with no one else's
  /// commands in between, or none of them if it couldn't take one. Fails if
  /// any of them failed.
  absl::Status Transaction(
      const std::vector<std::vector<std::string>>& commands);

  /// Runs the commands as one, as Transaction does, but only if each of the
  /// keys holds the value it's paired with: a Lua script checks them and runs
  /// the commands, in a single round trip, with no one else's commands in
  /// between. Returns the keys that don't, when it ran none of the commands.
  /// Fails if a command failed, which doesn't undo the others.
  absl::StatusOr<std::vector<std::string>> TransactionIf(
      const std::map<std::string, std::string>& holding,
      const std::vector<std::vector<std::string>>& commands);

  /// An entry of a stream: its id, and its fields and values, one after the
  /// other.
  struct Entry {
    std::string id;
    std::vector<std::string> fields;
  };

  /// Reads a stream, with XREAD or XREADGROUP on a single stream: its entries,
  /// or none when the command waited and nothing came. One that BLOCKs waits
  /// that long for Redis, and the timeout on top, and one that BLOCKs for 0
  /// as long as Redis does: for ever, until an entry comes.
  absl::StatusOr<std::vector<Entry>> Read(
      const std::vector<std::string>& words);

 private:
  Redis(redisContext* context, RedisTimeouts timeouts)
      : context_(context), timeouts_(timeouts) {}

  /// Whether to ask Redis at all: not while failing fast, and only once the
  /// connection's back, if it was lost.
  absl::Status Ready();
  /// The next reply, or why there's none.
  absl::StatusOr<std::unique_ptr<redisReply, decltype(&freeReplyObject)>>
  NextReply();
  /// Fails each call at once for the timeouts' fail_fast.
  void FailFast();

  redisContext* context_;
  RedisTimeouts timeouts_;
  std::chrono::steady_clock::time_point failing_until_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_REDIS_H_
