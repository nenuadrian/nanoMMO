#ifndef NANOMMO_BOT_BOT_METRICS_H_
#define NANOMMO_BOT_BOT_METRICS_H_

#include <chrono>
#include <memory>
#include <string>

#include "prometheus/family.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/registry.h"

namespace nanommo {

class BotMetrics {
 public:
  explicit BotMetrics(const std::string& transport);

  void ObserveAge(std::chrono::microseconds age);

  void SetConnected(int bots);

  /// A bot's login, from its first request to the login server to its token
  /// to enter the world, by what came of it: ok, refused, timeout when the
  /// login server didn't answer within LogIn's deadline, or failed. It's
  /// what players see, which the login server's own metrics can't, as a
  /// request that never reaches it, or never ends, is never counted there.
  void ObserveLogin(const char* result, std::chrono::nanoseconds time);

  const std::shared_ptr<prometheus::Registry>& registry() const {
    return registry_;
  }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  prometheus::Histogram& age_seconds_;
  prometheus::Gauge& connected_;
  prometheus::Family<prometheus::Histogram>& logins_;
};

}  // namespace nanommo

#endif  // NANOMMO_BOT_BOT_METRICS_H_
