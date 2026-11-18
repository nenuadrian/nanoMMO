#include "server/metrics.h"

#include <chrono>
#include <string>

#include "gtest/gtest.h"
#include "prometheus/text_serializer.h"

namespace {

std::string Scrape(const nanommo::Metrics& metrics) {
  return prometheus::TextSerializer().Serialize(metrics.registry()->Collect());
}

bool Has(const std::string& text, const std::string& line) {
  return text.find(line + "\n") != std::string::npos;
}

}  // namespace

TEST(Metrics, PutsEachTickInItsBucket) {
  nanommo::Metrics metrics;
  metrics.ObserveTick(std::chrono::microseconds(300));
  const std::string text = Scrape(metrics);

  EXPECT_TRUE(Has(text, "nanommo_tick_duration_seconds_count 1"));
  EXPECT_TRUE(
      Has(text, R"(nanommo_tick_duration_seconds_bucket{le="0.00025"} 0)"));
  EXPECT_TRUE(
      Has(text, R"(nanommo_tick_duration_seconds_bucket{le="0.0005"} 1)"));
}

TEST(Metrics, KeepsTicksOverTheBudgetApart) {
  nanommo::Metrics metrics;
  metrics.ObserveTick(std::chrono::milliseconds(60));
  metrics.ObserveTick(std::chrono::milliseconds(300));
  const std::string text = Scrape(metrics);

  EXPECT_TRUE(
      Has(text, R"(nanommo_tick_duration_seconds_bucket{le="0.05"} 0)"));
  EXPECT_TRUE(
      Has(text, R"(nanommo_tick_duration_seconds_bucket{le="0.075"} 1)"));
  EXPECT_TRUE(Has(text, R"(nanommo_tick_duration_seconds_bucket{le="0.5"} 2)"));
}

TEST(Metrics, CountsThePlayersDroppedByWhy) {
  nanommo::Metrics metrics;
  // Both are there before anyone's dropped, at zero.
  const std::string before = Scrape(metrics);
  EXPECT_TRUE(Has(before, R"(nanommo_players_dropped_total{why="lease"} 0)"));
  EXPECT_TRUE(Has(before, R"(nanommo_players_dropped_total{why="trade"} 0)"));
  metrics.AddDropped("lease");
  metrics.AddDropped("lease");
  metrics.AddDropped("trade");
  const std::string text = Scrape(metrics);

  EXPECT_TRUE(Has(text, R"(nanommo_players_dropped_total{why="lease"} 2)"));
  EXPECT_TRUE(Has(text, R"(nanommo_players_dropped_total{why="trade"} 1)"));
}

TEST(LoginMetrics, PutsEachRequestInItsBucketByWhatCameOfIt) {
  nanommo::LoginMetrics metrics;
  metrics.ObserveRequest("ok", std::chrono::milliseconds(60));
  metrics.ObserveRequest("failed", std::chrono::seconds(7));
  const std::string text =
      prometheus::TextSerializer().Serialize(metrics.registry()->Collect());

  EXPECT_TRUE(
      Has(text,
          R"(nanommo_login_duration_seconds_bucket{result="ok",le="0.05"} 0)"));
  EXPECT_TRUE(
      Has(text,
          R"(nanommo_login_duration_seconds_bucket{result="ok",le="0.1"} 1)"));
  // Seven seconds is past the five a player waits.
  EXPECT_TRUE(Has(
      text,
      R"(nanommo_login_duration_seconds_bucket{result="failed",le="5"} 0)"));
  EXPECT_TRUE(Has(
      text,
      R"(nanommo_login_duration_seconds_bucket{result="failed",le="10"} 1)"));
  // Nobody's been refused yet, which is there as none.
  EXPECT_TRUE(
      Has(text, R"(nanommo_login_duration_seconds_count{result="refused"} 0)"));
}
