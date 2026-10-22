#include "bot/bot_metrics.h"

#include <string>

namespace nanommo {

BotMetrics::BotMetrics(const std::string& transport)
    : registry_(std::make_shared<prometheus::Registry>()),
      age_seconds_(prometheus::BuildHistogram()
                       .Name("nanommo_bot_world_age_seconds")
                       .Help("How old each bot's newest world is")
                       .Register(*registry_)
                       .Add({{"transport", transport}},
                            prometheus::Histogram::BucketBoundaries{
                                0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07,
                                0.08, 0.09, 0.1,  0.11, 0.12, 0.13, 0.14,
                                0.15, 0.16, 0.17, 0.18, 0.19, 0.2,  0.25,
                                0.3,  0.4,  0.5,  0.75, 1,    2.5,  5})),
      connected_(prometheus::BuildGauge()
                     .Name("nanommo_bots_connected")
                     .Help("How many bots are connected to a world")
                     .Register(*registry_)
                     .Add({{"transport", transport}})) {}

void BotMetrics::ObserveAge(std::chrono::microseconds age) {
  age_seconds_.Observe(std::chrono::duration<double>(age).count());
}

void BotMetrics::SetConnected(int bots) { connected_.Set(bots); }

}  // namespace nanommo
