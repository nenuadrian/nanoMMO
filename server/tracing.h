#ifndef NANOMMO_SERVER_TRACING_H_
#define NANOMMO_SERVER_TRACING_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "prometheus/registry.h"

namespace nanommo {

/// Where a span is: the trace it's part of, by its 16 random bytes, and the
/// span itself, by its 8. A span id of zero is no span.
struct SpanContext {
  uint64_t trace_high = 0;
  uint64_t trace_low = 0;
  uint64_t span = 0;

  bool valid() const { return span != 0; }
  /// The trace's id, as 32 hex digits, as Tempo shows it.
  std::string trace_id() const;

  bool operator==(const SpanContext&) const = default;
};

/// The context as W3C Trace Context's traceparent writes it:
/// 00-<trace id>-<span id>-01.
std::string Traceparent(const SpanContext& context);

/// The context a traceparent names, or an InvalidArgument error for one that
/// isn't written as the standard says, or names no trace or no span.
absl::StatusOr<SpanContext> ParseTraceparent(std::string_view traceparent);

/// What a span's work is, numbered as OpenTelemetry numbers them: work inside
/// a process, a server's answer to a request, a client's call to another
/// service, and a message put on a queue, and taken off it.
enum class SpanKind {
  kInternal = 1,
  kServer = 2,
  kClient = 3,
  kProducer = 4,
  kConsumer = 5,
};

/// A piece of a trace's work, from when it starts to when it ends, or goes out
/// of scope. While it's open, it's its thread's current span, which the next
/// span started on the thread is part of. A span started while tracing is off
/// records nothing.
class Span {
 public:
  /// What a span records, which only tracing.cc knows.
  struct Data;

  /// A span that records nothing.
  Span();
  explicit Span(std::unique_ptr<Data> data);
  ~Span();

  /// The span it was, which the other doesn't record any more.
  Span(Span&& other) noexcept;
  Span(const Span&) = delete;
  Span& operator=(const Span&) = delete;

  bool recording() const { return data_ != nullptr; }
  /// No trace and no span when it isn't recording.
  SpanContext context() const;

  void SetAttribute(std::string_view key, std::string_view value);
  void SetAttribute(std::string_view key, int64_t value);
  /// Marks it as failed, and why.
  void SetError(std::string_view message);
  /// Another trace's span that this one's work was for too, as a batch's span
  /// is for each message in it, and has one parent.
  void AddLink(const SpanContext& other);

  /// Records it, and makes the span that was current before it current again.
  /// Going out of scope does the same.
  void End();

 private:
  std::unique_ptr<Data> data_;
  SpanContext previous_;
};

/// Sends a batch of spans, written as OTLP's JSON, and says whether it went.
using SendSpans = std::function<absl::Status(const std::string& json)>;

/// Posts each batch to an OpenTelemetry collector at the endpoint, such as
/// http://alloy:4318, at /v1/traces.
SendSpans PostTo(const std::string& endpoint);

/// The most spans that wait to be sent. A span that ends while that many are
/// waiting is dropped.
inline constexpr std::size_t kMostWaitingSpans = 4096;

/// Turns tracing on for this process: from now on, its spans are the
/// service's, and a thread of their own sends them, once a second, or as
/// soon as there's a batch of them. With a registry, it counts the spans it
/// drops there. Tracing is off until this is called, so tests, and a server
/// run without a collector, record nothing.
void StartTracing(std::string service, SendSpans send,
                  prometheus::Registry* registry = nullptr);

/// Sends the spans that are left, within two seconds, drops the rest, and turns
/// tracing off. Call it once the threads that start spans have stopped.
void StopTracing();

/// A span under the thread's current span, or the first of a new trace when
/// there's none.
Span StartSpan(std::string_view name, SpanKind kind);

/// A span under one from another process, or the first of a new trace when
/// the parent's no span, from start: a request's work can start before we
/// know which trace it's part of.
Span StartSpan(std::string_view name, SpanKind kind, const SpanContext& parent,
               absl::Time start = absl::Now());

/// Whether the thread has a current span: whether a call made now is made for
/// a traced request. The tick's calls aren't, and stop at this check.
bool InTrace();

/// A span under the thread's current span, or one that records nothing when
/// there's none. For calls to Redis and MySQL, which are traced when they're
/// made for a traced request, and not when the tick makes them.
Span StartChildSpan(std::string_view name, SpanKind kind);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TRACING_H_
