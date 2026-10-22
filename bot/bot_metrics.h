#ifndef NANOMMO_BOT_BOT_METRICS_H_
#define NANOMMO_BOT_BOT_METRICS_H_

#include <chrono>
#include <memory>
#include <string>

#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/registry.h"

namespace nanommo {

class BotMetrics {
 public:
  explicit BotMetrics(const std::string& transport);

  void ObserveAge(std::chrono::microseconds age);

  void SetConnected(int bots);

  const std::shared_ptr<prometheus::Registry>& registry() const {
    return registry_;
  }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  prometheus::Histogram& age_seconds_;
  prometheus::Gauge& connected_;
};

}  // namespace nanommo

#endif  // NANOMMO_BOT_BOT_METRICS_H_
