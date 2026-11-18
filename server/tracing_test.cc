#include "server/tracing.h"

#include <httplib.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "prometheus/registry.h"

namespace {

using ::nanommo::ParseTraceparent;
using ::nanommo::Span;
using ::nanommo::SpanContext;
using ::nanommo::SpanKind;
using ::nanommo::StartChildSpan;
using ::nanommo::StartSpan;
using ::nanommo::Traceparent;

// W3C Trace Context's own example.
constexpr char kExample[] =
    "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01";

// Turns tracing on, as the service "test", for one test, and keeps each batch
// it sends, as JSON.
class TracingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nanommo::StartTracing(
        "test",
        [this](const std::string& json) {
          const std::lock_guard lock(mutex_);
          sent_.push_back(nlohmann::json::parse(json, nullptr, false));
          return absl::OkStatus();
        },
        &registry_);
  }
  void TearDown() override { nanommo::StopTracing(); }

  // Every span sent, once tracing has stopped.
  std::vector<nlohmann::json> Spans() {
    nanommo::StopTracing();
    std::vector<nlohmann::json> spans;
    for (const nlohmann::json& batch : sent_) {
      for (const nlohmann::json& span :
           batch["resourceSpans"][0]["scopeSpans"][0]["spans"]) {
        spans.push_back(span);
      }
    }
    return spans;
  }

  prometheus::Registry registry_;
  std::mutex mutex_;
  std::vector<nlohmann::json> sent_;
};

}  // namespace

TEST(Traceparent, ReadsAndWritesTheStandardsExample) {
  const absl::StatusOr<SpanContext> context = ParseTraceparent(kExample);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->trace_id(), "4bf92f3577b34da6a3ce929d0e0e4736");
  EXPECT_EQ(context->span, 0x00f067aa0ba902b7u);
  EXPECT_EQ(Traceparent(*context), kExample);
}

TEST(Traceparent, RefusesWhatIsNotWrittenAsTheStandardSays) {
  // Each of these is the example with one thing wrong, which the example
  // itself, above, doesn't have.
  for (const char* wrong : {
           // Uppercase hex.
           "00-4BF92F3577B34DA6A3CE929D0E0E4736-00f067aa0ba902b7-01",
           // A version there isn't.
           "01-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
           // A trace id a digit short, and one a digit long.
           "00-4bf92f3577b34da6a3ce929d0e0e473-00f067aa0ba902b7-01",
           "00-4bf92f3577b34da6a3ce929d0e0e47366-00f067aa0ba902b7-01",
           // A character that isn't hex.
           "00-4bf92f3577b34da6a3ce929d0e0e473g-00f067aa0ba902b7-01",
           // A span id with a sign, which a parser of numbers would take.
           "00-4bf92f3577b34da6a3ce929d0e0e4736-+0f067aa0ba902b7-01",
           // A part missing, and one too many.
           "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7",
           "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01-00",
           // Spaces round it.
           " 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01 ",
           // No trace, and no span.
           "00-00000000000000000000000000000000-00f067aa0ba902b7-01",
           "00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-01",
           "",
       }) {
    EXPECT_TRUE(absl::IsInvalidArgument(ParseTraceparent(wrong).status()))
        << wrong;
  }
}

TEST(Tracing, IsOffUntilItStarts) {
  Span span = StartSpan("login", SpanKind::kServer);
  EXPECT_FALSE(span.recording());
  EXPECT_FALSE(span.context().valid());
  EXPECT_FALSE(StartChildSpan("redis GET", SpanKind::kClient).recording());
}

TEST_F(TracingTest, SendsEachSpanAsOtlpJson) {
  SpanContext context;
  {
    Span span = StartSpan("login", SpanKind::kServer);
    span.SetAttribute("nanommo.account", int64_t{7});
    span.SetAttribute("nanommo.result", "logged in");
    span.SetError("MySQL is down");
    context = span.context();
  }
  const std::vector<nlohmann::json> spans = Spans();
  ASSERT_EQ(spans.size(), 1u);
  const nlohmann::json& span = spans.front();
  EXPECT_EQ(span["name"], "login");
  EXPECT_EQ(span["traceId"], context.trace_id());
  EXPECT_EQ(span["spanId"],
            absl::StrCat(absl::Hex(context.span, absl::kZeroPad16)));
  EXPECT_FALSE(span.contains("parentSpanId"));
  EXPECT_EQ(span["kind"], 2);
  EXPECT_EQ(span["attributes"][0]["key"], "nanommo.account");
  EXPECT_EQ(span["attributes"][0]["value"]["intValue"], "7");
  EXPECT_EQ(span["attributes"][1]["value"]["stringValue"], "logged in");
  EXPECT_EQ(span["status"]["code"], 2);
  EXPECT_EQ(span["status"]["message"], "MySQL is down");
  // Nanoseconds since 1970, as strings, the end after the start.
  const std::string start = span["startTimeUnixNano"];
  const std::string end = span["endTimeUnixNano"];
  EXPECT_EQ(start.size(), 19u);
  EXPECT_LE(start, end);
  EXPECT_EQ(
      sent_.front()["resourceSpans"][0]["resource"]["attributes"][0],
      nlohmann::json::parse(
          R"({"key": "service.name", "value": {"stringValue": "test"}})"));
}

TEST_F(TracingTest, ASpanStartedInsideAnotherIsItsChild) {
  Span login = StartSpan("login", SpanKind::kServer);
  Span lookup = StartSpan("mysql SELECT", SpanKind::kClient);
  EXPECT_EQ(lookup.context().trace_id(), login.context().trace_id());
  EXPECT_NE(lookup.context().span, login.context().span);
  lookup.End();
  // Once the child's ended, its parent's current again.
  Span token = StartChildSpan("redis SET", SpanKind::kClient);
  EXPECT_TRUE(token.recording());
  token.End();
  login.End();

  const std::vector<nlohmann::json> spans = Spans();
  ASSERT_EQ(spans.size(), 3u);
  EXPECT_EQ(spans[0]["parentSpanId"], spans[2]["spanId"]);
  EXPECT_EQ(spans[1]["parentSpanId"], spans[2]["spanId"]);
  EXPECT_EQ(spans[0]["kind"], 3);
}

TEST_F(TracingTest, AChildSpanNeedsAParent) {
  EXPECT_FALSE(StartChildSpan("redis GET", SpanKind::kClient).recording());
  // With one, the same call records.
  Span hello = StartSpan("hello", SpanKind::kServer);
  EXPECT_TRUE(StartChildSpan("redis GET", SpanKind::kClient).recording());
}

TEST_F(TracingTest, LinksToTheOtherSpansItWasFor) {
  SpanContext first;
  SpanContext second;
  {
    Span one = StartSpan("publish trade", SpanKind::kProducer);
    first = one.context();
  }
  {
    Span two = StartSpan("publish trade", SpanKind::kProducer);
    second = two.context();
  }
  {
    Span batch = StartSpan("save trades", SpanKind::kConsumer, first);
    batch.AddLink(second);
  }
  const std::vector<nlohmann::json> spans = Spans();
  ASSERT_EQ(spans.size(), 3u);
  // Under the first, in its trace, and linked to the second, in another.
  EXPECT_EQ(spans[2]["traceId"], first.trace_id());
  EXPECT_EQ(spans[2]["parentSpanId"], spans[0]["spanId"]);
  EXPECT_EQ(spans[2]["links"],
            nlohmann::json::parse(absl::StrCat(
                R"([{"traceId": ")", second.trace_id(), R"(", "spanId": ")",
                spans[1]["spanId"].get<std::string>(), R"("}])")));
  EXPECT_NE(first.trace_id(), second.trace_id());
}

TEST_F(TracingTest, EachTraceAndSpanHasAnIdOfItsOwn) {
  std::set<std::string> traces;
  std::set<uint64_t> spans;
  for (int i = 0; i < 1000; ++i) {
    Span span = StartSpan("hello", SpanKind::kServer);
    traces.insert(span.context().trace_id());
    spans.insert(span.context().span);
  }
  EXPECT_EQ(traces.size(), 1000u);
  EXPECT_EQ(spans.size(), 1000u);
  EXPECT_FALSE(traces.contains(std::string(32, '0')));
  EXPECT_FALSE(spans.contains(0));
}

TEST(Tracing, DropsSpansWhileTheQueueIsFull) {
  // The collector takes the first batch, and holds on to it until we let it
  // go, while the queue fills up behind it.
  std::mutex mutex;
  std::condition_variable changed;
  bool sending = false;
  bool let_go = false;
  std::size_t sent = 0;
  prometheus::Registry registry;
  nanommo::StartTracing(
      "test",
      [&](const std::string& json) {
        std::unique_lock lock(mutex);
        sending = true;
        changed.notify_all();
        changed.wait(lock, [&] { return let_go; });
        sent += nlohmann::json::parse(
                    json, nullptr,
                    false)["resourceSpans"][0]["scopeSpans"][0]["spans"]
                    .size();
        return absl::OkStatus();
      },
      &registry);
  StartSpan("first", SpanKind::kServer).End();
  {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return sending; });
  }
  for (std::size_t i = 0; i < nanommo::kMostWaitingSpans + 10; ++i) {
    StartSpan("hello", SpanKind::kServer).End();
  }
  {
    const std::lock_guard lock(mutex);
    let_go = true;
  }
  changed.notify_all();
  nanommo::StopTracing();

  EXPECT_EQ(sent, 1 + nanommo::kMostWaitingSpans);
  const auto families = registry.Collect();
  ASSERT_EQ(families.size(), 1u);
  EXPECT_EQ(families[0].name, "nanommo_spans_dropped_total");
  EXPECT_EQ(families[0].metric[0].counter.value, 10.0);
}

TEST(Tracing, GivesUpOnWhatsLeftAfterTwoSecondsAsItStops) {
  // A collector that takes half a second over each batch.
  std::mutex mutex;
  std::size_t sent = 0;
  prometheus::Registry registry;
  nanommo::StartTracing(
      "test",
      [&](const std::string& json) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const std::lock_guard lock(mutex);
        sent += nlohmann::json::parse(
                    json, nullptr,
                    false)["resourceSpans"][0]["scopeSpans"][0]["spans"]
                    .size();
        return absl::OkStatus();
      },
      &registry);
  // Eight batches, which would take four seconds.
  for (std::size_t i = 0; i < nanommo::kMostWaitingSpans; ++i) {
    StartSpan("hello", SpanKind::kServer).End();
  }
  const auto stopping = std::chrono::steady_clock::now();
  nanommo::StopTracing();
  const auto stopped = std::chrono::steady_clock::now();

  // Within the two seconds, and the batch it was sending then.
  EXPECT_LT(stopped - stopping, std::chrono::milliseconds(3500));
  EXPECT_GT(sent, 0u);
  EXPECT_LT(sent, nanommo::kMostWaitingSpans);
  // What it didn't send, it counts.
  EXPECT_EQ(
      static_cast<double>(sent) + registry.Collect()[0].metric[0].counter.value,
      static_cast<double>(nanommo::kMostWaitingSpans));
}

TEST(Tracing, PostsToACollector) {
  // A collector of our own, on a port the system picks.
  httplib::Server collector;
  std::mutex mutex;
  std::vector<std::string> bodies;
  collector.Post("/v1/traces", [&](const httplib::Request& request,
                                   httplib::Response& response) {
    // As a collector that's overloaded answers.
    if (request.body == "refuse") {
      response.status = 503;
      return;
    }
    const std::lock_guard lock(mutex);
    EXPECT_EQ(request.get_header_value("Content-Type"), "application/json");
    bodies.push_back(request.body);
  });
  const int port = collector.bind_to_any_port("127.0.0.1");
  ASSERT_GT(port, 0);
  std::thread listening([&collector] { collector.listen_after_bind(); });
  collector.wait_until_ready();

  const nanommo::SendSpans post =
      nanommo::PostTo(absl::StrCat("http://127.0.0.1:", port));
  nanommo::StartTracing("test", post);
  StartSpan("login", SpanKind::kServer).End();
  nanommo::StopTracing();
  EXPECT_FALSE(post("refuse").ok());
  collector.stop();
  listening.join();

  ASSERT_EQ(bodies.size(), 1u);
  const nlohmann::json batch = nlohmann::json::parse(bodies[0], nullptr, false);
  EXPECT_EQ(batch["resourceSpans"][0]["scopeSpans"][0]["spans"][0]["name"],
            "login");
  // Nobody's listening on a port it's just stopped listening on.
  EXPECT_TRUE(absl::IsUnavailable(post("{}")));
}
