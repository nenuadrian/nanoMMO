#ifndef NANOMMO_SERVER_REDIS_H_
#define NANOMMO_SERVER_REDIS_H_

#include <hiredis.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace nanommo {

class Redis {
 public:
  /// Connects to Redis, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Redis>> Create(const std::string& host,
                                                       int port = 6379);
  ~Redis();

  Redis(const Redis&) = delete;
  Redis& operator=(const Redis&) = delete;

  /// Returns the reply as text, or a NotFound error when Redis says nil. A
  /// command that loses the connection fails, and the next one reconnects.
  absl::StatusOr<std::string> Command(const std::vector<std::string>& words);

  /// Sends all the commands before reading any reply, so they share a single
  /// round trip. Each reply comes back as a list of values, empty for nil.
  absl::StatusOr<std::vector<std::vector<std::string>>> Pipeline(
      const std::vector<std::vector<std::string>>& commands);

  /// An entry of a stream: its id, and its fields and values, one after the
  /// other.
  struct Entry {
    std::string id;
    std::vector<std::string> fields;
  };

  /// Reads a stream, with XREAD or XREADGROUP on a single stream: its entries,
  /// or none when the command waited and nothing came.
  absl::StatusOr<std::vector<Entry>> Read(
      const std::vector<std::string>& words);

 private:
  explicit Redis(redisContext* context) : context_(context) {}

  redisContext* context_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_REDIS_H_
