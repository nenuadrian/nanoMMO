#ifndef NANOMMO_SERVER_METRICS_H_
#define NANOMMO_SERVER_METRICS_H_

#include <chrono>
#include <cstddef>
#include <memory>

#include "prometheus/counter.h"
#include "prometheus/family.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/registry.h"

namespace nanommo {

class Metrics {
 public:
  Metrics();

  void SetPlayers(const char* land, std::size_t count);

  void ObserveTick(std::chrono::nanoseconds time);

  void ObserveSave(std::chrono::nanoseconds time);

  void AddBytesSent(std::size_t bytes);

  void AddMessageReceived();

  /// A line of chat, by what became of it: said, or refused.
  void AddChatLine(const char* result);

  /// A story a player asked for, by what became of it: told, refused, or
  /// failed, when the storyteller couldn't tell it.
  void AddStory(const char* result);

  /// From when the player asked to when the story was told.
  void ObserveStory(std::chrono::nanoseconds time);

  const std::shared_ptr<prometheus::Registry>& registry() const {
    return registry_;
  }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  prometheus::Family<prometheus::Gauge>& players_;
  prometheus::Histogram& tick_seconds_;
  prometheus::Histogram& save_seconds_;
  prometheus::Counter& bytes_sent_;
  prometheus::Counter& messages_received_;
  prometheus::Family<prometheus::Counter>& chat_lines_;
  prometheus::Family<prometheus::Counter>& stories_;
  prometheus::Histogram& story_seconds_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_METRICS_H_
