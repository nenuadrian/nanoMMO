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
