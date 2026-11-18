#include "bot/bot_metrics.h"

#include <chrono>
#include <string>

namespace nanommo {

namespace {

prometheus::Histogram::BucketBoundaries LoginBuckets() {
  return {0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 20};
}

}  // namespace

BotMetrics::BotMetrics(const std::string& transport)
    : registry_(std::make_shared<prometheus::Registry>()),
      age_seconds_(prometheus::BuildHistogram()
                       .Name("nanommo_bot_world_age_seconds")
                       .Help("How old each bot's newest world is")
                       .Register(*registry_)
                       .Add({{"transport", transport}},
                            prometheus::Histogram::BucketBoundaries{
                                0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08,
                                0.09, 0.1,  0.11, 0.12, 0.13, 0.14, 0.15, 0.16,
                                0.17, 0.18, 0.19, 0.2,  0.25, 0.3,  0.4,  0.5,
                                0.75, 1,    1.5,  2,    2.5,  3,    4,    5,
                                6,    8,    10,   15,   20,   30})),
      connected_(prometheus::BuildGauge()
                     .Name("nanommo_bots_connected")
                     .Help("How many bots are connected to a world")
                     .Register(*registry_)
                     .Add({{"transport", transport}})),
      logins_(prometheus::BuildHistogram()
                  .Name("nanommo_bot_login_duration_seconds")
                  .Help("How long each bot's login took, by what came of it")
                  .Register(*registry_)) {
  for (const char* result : {"ok", "refused", "timeout", "failed"}) {
    logins_.Add({{"result", result}}, LoginBuckets());
  }
}

void BotMetrics::ObserveAge(std::chrono::microseconds age) {
  age_seconds_.Observe(std::chrono::duration<double>(age).count());
}

void BotMetrics::SetConnected(int bots) { connected_.Set(bots); }

void BotMetrics::ObserveLogin(const char* result,
                              std::chrono::nanoseconds time) {
  logins_.Add({{"result", result}}, LoginBuckets())
      .Observe(std::chrono::duration<double>(time).count());
}

}  // namespace nanommo
