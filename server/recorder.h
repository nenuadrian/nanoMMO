#ifndef NANOMMO_SERVER_RECORDER_H_
#define NANOMMO_SERVER_RECORDER_H_

#include <chrono>
#include <functional>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "server/redis.h"

namespace nanommo {

/// Takes the game's events from the Redis stream the worlds add them to, a
/// batch at a time, to the sink, which puts them into ClickHouse.
class Recorder {
 public:
  /// Stores a batch of rows, one a line, or says why it couldn't.
  using Sink = std::function<absl::Status(std::string_view rows)>;

  /// The consumer group the recorders read the stream as. Redis gives each
  /// entry to one consumer of the group, and remembers which it's given out
  /// and not had acknowledged.
  static constexpr char kGroup[] = "recorder";

  /// The most rows a batch takes.
  static constexpr int kBatch = 10'000;

  /// Waits up to wait each time for events to come, as the consumer.
  Recorder(Redis& redis, std::string consumer, Sink sink,
           std::chrono::milliseconds wait = std::chrono::seconds(1));

  /// Records a batch: first what it took before and couldn't store, then
  /// what's new. Returns how many it stored.
  absl::StatusOr<int> Record();

 private:
  Redis& redis_;
  std::string consumer_;
  Sink sink_;
  std::chrono::milliseconds wait_;
  bool grouped_ = false;
  /// Whether to look at what it took before first.
  bool pending_ = true;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_RECORDER_H_
