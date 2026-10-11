#ifndef NANOMMO_SERVER_METRICS_H_
#define NANOMMO_SERVER_METRICS_H_

#include <chrono>
#include <cstddef>
#include <memory>

#include "prometheus/counter.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/registry.h"

namespace nanommo {

class Metrics {
 public:
  Metrics();

  void SetPlayers(std::size_t count);

  void ObserveTick(std::chrono::nanoseconds time);

  void AddBytesSent(std::size_t bytes);

  void AddMessageReceived();

  const std::shared_ptr<prometheus::Registry>& registry() const {
    return registry_;
  }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  prometheus::Gauge& players_;
  prometheus::Histogram& tick_seconds_;
  prometheus::Counter& bytes_sent_;
  prometheus::Counter& messages_received_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_METRICS_H_
