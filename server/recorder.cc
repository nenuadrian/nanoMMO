#include "server/recorder.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "server/events.h"

namespace nanommo {

Recorder::Recorder(Redis& redis, std::string consumer, Sink sink,
                   std::chrono::milliseconds wait)
    : redis_(redis),
      consumer_(std::move(consumer)),
      sink_(std::move(sink)),
      wait_(wait) {}

absl::StatusOr<int> Recorder::Record() {
  if (!grouped_) {
    // From the start of the stream, which MKSTREAM makes if it isn't there. A
    // group that's there already is fine.
    const absl::StatusOr<std::string> created = redis_.Command(
        {"XGROUP", "CREATE", kEventStream, kGroup, "0", "MKSTREAM"});
    if (!created.ok() &&
        !absl::StrContains(created.status().message(), "BUSYGROUP")) {
      return created.status();
    }
    grouped_ = true;
  }
  // Zero reads what this consumer took and hasn't acknowledged, and > what
  // nobody in the group has taken yet, waiting for it.
  std::vector<std::string> words = {"XREADGROUP", "GROUP", kGroup, consumer_};
  words.insert(words.end(), {"COUNT", std::to_string(kBatch)});
  if (!pending_) {
    words.insert(words.end(), {"BLOCK", std::to_string(wait_.count())});
  }
  words.insert(words.end(), {"STREAMS", kEventStream, pending_ ? "0" : ">"});
  const absl::StatusOr<std::vector<Redis::Entry>> entries = redis_.Read(words);
  if (!entries.ok()) {
    return entries.status();
  }
  if (entries->empty()) {
    pending_ = false;
    return 0;
  }
  std::string rows;
  std::vector<std::string> ack = {"XACK", kEventStream, kGroup};
  for (const Redis::Entry& entry : *entries) {
    // Fields and values take turns.
    for (std::size_t i = 0; i + 1 < entry.fields.size(); i += 2) {
      if (entry.fields[i] == "row") {
        rows += entry.fields[i + 1];
        rows += '\n';
      }
    }
    ack.push_back(entry.id);
  }
  if (!rows.empty()) {
    const absl::Status stored = sink_(rows);
    if (!stored.ok()) {
      // They stay taken and unacknowledged, for the next try.
      pending_ = true;
      return stored;
    }
  }
  const absl::StatusOr<std::string> acked = redis_.Command(ack);
  if (!acked.ok()) {
    return acked.status();
  }
  return static_cast<int>(entries->size());
}

}  // namespace nanommo
