#include "server/metrics.h"

namespace nanommo {

namespace {

prometheus::Histogram::BucketBoundaries TimeBuckets() {
  return {0.0001, 0.00025, 0.0005, 0.001, 0.0025, 0.005, 0.01, 0.02,
          0.03,   0.04,    0.05,   0.075, 0.1,    0.25,  0.5,  1};
}

}  // namespace

Metrics::Metrics()
    : registry_(std::make_shared<prometheus::Registry>()),
      players_(prometheus::BuildGauge()
                   .Name("nanommo_players_online")
                   .Help("Players connected to the server")
                   .Register(*registry_)
                   .Add({})),
      tick_seconds_(prometheus::BuildHistogram()
                        .Name("nanommo_tick_duration_seconds")
                        .Help("Time spent in each tick")
                        .Register(*registry_)
                        .Add({}, TimeBuckets())),
      save_seconds_(prometheus::BuildHistogram()
                        .Name("nanommo_save_duration_seconds")
                        .Help("Time spent saving players, in each tick")
                        .Register(*registry_)
                        .Add({}, TimeBuckets())),
      bytes_sent_(prometheus::BuildCounter()
                      .Name("nanommo_bytes_sent_total")
                      .Help("Bytes queued to send to clients")
                      .Register(*registry_)
                      .Add({})),
      messages_received_(prometheus::BuildCounter()
                             .Name("nanommo_messages_received_total")
                             .Help("Messages received from clients")
                             .Register(*registry_)
                             .Add({})) {}

void Metrics::SetPlayers(std::size_t count) {
  players_.Set(static_cast<double>(count));
}

void Metrics::ObserveTick(std::chrono::nanoseconds time) {
  tick_seconds_.Observe(std::chrono::duration<double>(time).count());
}

void Metrics::ObserveSave(std::chrono::nanoseconds time) {
  save_seconds_.Observe(std::chrono::duration<double>(time).count());
}

void Metrics::AddBytesSent(std::size_t bytes) {
  bytes_sent_.Increment(static_cast<double>(bytes));
}

void Metrics::AddMessageReceived() { messages_received_.Increment(); }

}  // namespace nanommo
