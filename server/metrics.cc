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
                   .Help("Players connected to the server, by land")
                   .Register(*registry_)),
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
                             .Add({})),
      chat_lines_(prometheus::BuildCounter()
                      .Name("nanommo_chat_lines_total")
                      .Help("Lines of chat, said or refused")
                      .Register(*registry_)) {}

void Metrics::SetPlayers(const char* land, std::size_t count) {
  // Add returns the land's gauge if it has one already.
  players_.Add({{"land", land}}).Set(static_cast<double>(count));
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

void Metrics::AddChatLine(const char* result) {
  chat_lines_.Add({{"result", result}}).Increment();
}

}  // namespace nanommo
