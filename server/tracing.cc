#include "server/tracing.h"

#include <httplib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "nlohmann/json.hpp"
#include "prometheus/counter.h"

namespace nanommo {

struct Span::Data {
  std::string name;
  SpanKind kind = SpanKind::kInternal;
  SpanContext context;
  uint64_t parent = 0;
  int64_t start = 0;
  int64_t end = 0;
  std::vector<std::pair<std::string, std::variant<std::string, int64_t>>>
      attributes;
  bool failed = false;
  std::string error;
};

namespace {

// A batch goes once a second, or as soon as there are this many.
constexpr std::size_t kBatch = 512;
constexpr std::chrono::seconds kSendEvery{1};
// What's left as tracing stops goes within this long, or is dropped, so that a
// collector that's slow, or gone, can't hold up a server that's stopping.
constexpr std::chrono::seconds kStopWithin{2};

std::string Hex(uint64_t id) {
  return absl::StrCat(absl::Hex(id, absl::kZeroPad16));
}

// An id nobody else has made: random, and never zero, which is no id.
uint64_t RandomId() {
  // Seeded from the system once per thread, and quick after that.
  thread_local absl::BitGen random;
  uint64_t id = 0;
  while (id == 0) {
    id = absl::Uniform<uint64_t>(random);
  }
  return id;
}

bool IsLowerHex(std::string_view text) {
  return std::ranges::all_of(text, [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

nlohmann::json Attribute(std::string_view key,
                         const std::variant<std::string, int64_t>& value) {
  nlohmann::json written;
  if (const auto* text = std::get_if<std::string>(&value)) {
    written["stringValue"] = *text;
  } else if (const auto* number = std::get_if<int64_t>(&value)) {
    // OTLP's JSON writes a 64-bit number as a string.
    written["intValue"] = absl::StrCat(*number);
  }
  return {{"key", key}, {"value", written}};
}

// The spans as OTLP's JSON: a resource, the service, with a scope, ours, with
// the spans. Ids go in hex, kinds as numbers, and times as strings of
// nanoseconds since 1970.
std::string Json(const std::string& service,
                 std::span<const Span::Data> spans) {
  nlohmann::json written = nlohmann::json::array();
  for (const Span::Data& span : spans) {
    nlohmann::json attributes = nlohmann::json::array();
    for (const auto& [key, value] : span.attributes) {
      attributes.push_back(Attribute(key, value));
    }
    nlohmann::json json = {
        {"traceId", span.context.trace_id()},
        {"spanId", Hex(span.context.span)},
        {"name", span.name},
        {"kind", static_cast<int>(span.kind)},
        {"startTimeUnixNano", absl::StrCat(span.start)},
        {"endTimeUnixNano", absl::StrCat(span.end)},
        {"attributes", std::move(attributes)},
    };
    if (span.parent != 0) {
      json["parentSpanId"] = Hex(span.parent);
    }
    // Code 2 is an error.
    if (span.failed) {
      json["status"] = {{"code", 2}, {"message", span.error}};
    }
    written.push_back(std::move(json));
  }
  nlohmann::json resource;
  resource["attributes"] = nlohmann::json::array(
      {Attribute("service.name", std::variant<std::string, int64_t>(service))});
  nlohmann::json scope;
  scope["scope"] = {{"name", "nanommo"}};
  scope["spans"] = std::move(written);
  nlohmann::json batch;
  batch["resource"] = std::move(resource);
  batch["scopeSpans"] = nlohmann::json::array({std::move(scope)});
  nlohmann::json request;
  request["resourceSpans"] = nlohmann::json::array({std::move(batch)});
  // dump would throw at bytes that aren't UTF-8, and replaces them instead.
  return request.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

// Takes the spans as they end, from any thread, and sends them from its own,
// so a span ending never waits on the collector.
class Exporter {
 public:
  Exporter(std::string service, SendSpans send, prometheus::Counter* dropped)
      : service_(std::move(service)),
        send_(std::move(send)),
        dropped_(dropped),
        thread_([this] { Run(); }) {}

  ~Exporter() {
    {
      const std::lock_guard lock(mutex_);
      stopping_ = true;
      give_up_at_ = std::chrono::steady_clock::now() + kStopWithin;
    }
    wake_.notify_one();
    thread_.join();
  }

  Exporter(const Exporter&) = delete;
  Exporter& operator=(const Exporter&) = delete;

  void Take(Span::Data span) {
    std::unique_lock lock(mutex_);
    // The span may be the tick thread's, which mustn't wait, so when the
    // queue's full, it's dropped.
    if (waiting_.size() >= kMostWaitingSpans) {
      lock.unlock();
      Drop(1);
      return;
    }
    waiting_.push_back(std::move(span));
    const bool batch = waiting_.size() >= kBatch;
    lock.unlock();
    if (batch) {
      wake_.notify_one();
    }
  }

 private:
  void Run() {
    std::unique_lock lock(mutex_);
    while (!stopping_ || !waiting_.empty()) {
      wake_.wait_for(lock, kSendEvery,
                     [this] { return stopping_ || waiting_.size() >= kBatch; });
      std::vector<Span::Data> spans;
      spans.swap(waiting_);
      const auto give_up_at = give_up_at_;
      lock.unlock();
      for (std::size_t first = 0; first < spans.size(); first += kBatch) {
        if (std::chrono::steady_clock::now() > give_up_at) {
          Drop(spans.size() - first);
          break;
        }
        const std::span<const Span::Data> batch =
            std::span<const Span::Data>(spans).subspan(
                first, std::min(kBatch, spans.size() - first));
        Report(send_(Json(service_, batch)), batch.size());
      }
      lock.lock();
    }
  }

  // A batch that didn't go is dropped, as the spans after it keep coming.
  void Report(const absl::Status& sent, std::size_t spans) {
    if (!sent.ok()) {
      Drop(spans);
    }
    // Once when it changes, not with each batch.
    const std::string news = sent.ok() ? "" : sent.ToString();
    if (news != said_) {
      said_ = news;
      std::cerr << "nanommo tracing: "
                << (sent.ok() ? "sending spans again" : news) << "\n";
    }
  }

  void Drop(std::size_t spans) {
    if (dropped_ != nullptr) {
      dropped_->Increment(static_cast<double>(spans));
    }
  }

  const std::string service_;
  const SendSpans send_;
  prometheus::Counter* const dropped_;
  std::string said_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::vector<Span::Data> waiting_;
  bool stopping_ = false;
  std::chrono::steady_clock::time_point give_up_at_ =
      std::chrono::steady_clock::time_point::max();
  // Last, so the rest is ready before it runs.
  std::thread thread_;
};

// The exporter while tracing is on, and nullptr while it's off.
std::atomic<Exporter*> exporter{nullptr};

// The span that's open on this thread, which the next one started on it is
// part of.
thread_local SpanContext current;

Span Start(std::string_view name, SpanKind kind, const SpanContext& parent,
           absl::Time start) {
  if (exporter.load() == nullptr) {
    return Span();
  }
  auto data = std::make_unique<Span::Data>();
  data->name = name;
  data->kind = kind;
  data->start = absl::ToUnixNanos(start);
  // A child is in its parent's trace, and the first span of a trace makes it.
  if (parent.valid()) {
    data->context = parent;
    data->parent = parent.span;
  } else {
    data->context.trace_high = RandomId();
    data->context.trace_low = RandomId();
  }
  data->context.span = RandomId();
  return Span(std::move(data));
}

}  // namespace

std::string SpanContext::trace_id() const {
  return absl::StrCat(Hex(trace_high), Hex(trace_low));
}

std::string Traceparent(const SpanContext& context) {
  // The flags at the end, 01, say the trace is being kept.
  return absl::StrCat("00-", context.trace_id(), "-", Hex(context.span), "-01");
}

absl::StatusOr<SpanContext> ParseTraceparent(std::string_view traceparent) {
  // Version 00, the only one there is, has four parts, in lowercase hex.
  const std::vector<std::string_view> parts = absl::StrSplit(traceparent, '-');
  if (parts.size() != 4 || parts[0] != "00" || parts[1].size() != 32 ||
      parts[2].size() != 16 || parts[3].size() != 2 ||
      !std::ranges::all_of(parts, IsLowerHex)) {
    return absl::InvalidArgumentError(
        absl::StrCat("not a traceparent: ", traceparent));
  }
  SpanContext context;
  if (!absl::SimpleHexAtoi(parts[1].substr(0, 16), &context.trace_high) ||
      !absl::SimpleHexAtoi(parts[1].substr(16), &context.trace_low) ||
      !absl::SimpleHexAtoi(parts[2], &context.span)) {
    return absl::InvalidArgumentError(
        absl::StrCat("not a traceparent: ", traceparent));
  }
  // All zeros is no trace, and no span.
  if ((context.trace_high == 0 && context.trace_low == 0) ||
      context.span == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("a traceparent with no trace: ", traceparent));
  }
  return context;
}

Span::Span() = default;

Span::Span(std::unique_ptr<Data> data) : data_(std::move(data)) {
  previous_ = current;
  current = data_->context;
}

Span::Span(Span&& other) noexcept = default;

Span::~Span() { End(); }

SpanContext Span::context() const {
  return data_ != nullptr ? data_->context : SpanContext();
}

void Span::SetAttribute(std::string_view key, std::string_view value) {
  if (data_ != nullptr) {
    data_->attributes.emplace_back(key, std::string(value));
  }
}

void Span::SetAttribute(std::string_view key, int64_t value) {
  if (data_ != nullptr) {
    data_->attributes.emplace_back(key, value);
  }
}

void Span::SetError(std::string_view message) {
  if (data_ != nullptr) {
    data_->failed = true;
    data_->error = message;
  }
}

void Span::End() {
  if (data_ == nullptr) {
    return;
  }
  data_->end = absl::ToUnixNanos(absl::Now());
  current = previous_;
  if (Exporter* const sending = exporter.load(); sending != nullptr) {
    sending->Take(std::move(*data_));
  }
  data_.reset();
}

SendSpans PostTo(const std::string& endpoint) {
  return [endpoint](const std::string& json) -> absl::Status {
    httplib::Client http(endpoint);
    http.set_connection_timeout(1);
    http.set_read_timeout(2);
    const httplib::Result result =
        http.Post("/v1/traces", json, "application/json");
    if (!result) {
      return absl::UnavailableError(
          absl::StrCat("couldn't reach ", endpoint, ": ",
                       httplib::to_string(result.error())));
    }
    if (result->status != 200) {
      return absl::InternalError(absl::StrCat(
          endpoint, " answered ", result->status, ": ", result->body));
    }
    return absl::OkStatus();
  };
}

void StartTracing(std::string service, SendSpans send,
                  prometheus::Registry* registry) {
  prometheus::Counter* dropped = nullptr;
  if (registry != nullptr) {
    dropped = &prometheus::BuildCounter()
                   .Name("nanommo_spans_dropped_total")
                   .Help(
                       "Spans that ended while the queue was full, or "
                       "that the collector didn't take")
                   .Register(*registry)
                   .Add({});
  }
  delete exporter.exchange(
      new Exporter(std::move(service), std::move(send), dropped));
}

void StopTracing() { delete exporter.exchange(nullptr); }

Span StartSpan(std::string_view name, SpanKind kind) {
  return Start(name, kind, current, absl::Now());
}

Span StartSpan(std::string_view name, SpanKind kind, const SpanContext& parent,
               absl::Time start) {
  return Start(name, kind, parent, start);
}

bool InTrace() { return current.valid(); }

Span StartChildSpan(std::string_view name, SpanKind kind) {
  if (!InTrace()) {
    return Span();
  }
  return Start(name, kind, current, absl::Now());
}

}  // namespace nanommo
