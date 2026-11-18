#include "server/redis.h"

#include <sys/time.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "server/tracing.h"

namespace nanommo {

namespace {

using Reply = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

timeval TimevalOf(std::chrono::milliseconds time) {
  return {.tv_sec = static_cast<decltype(timeval::tv_sec)>(time.count() / 1000),
          .tv_usec = static_cast<decltype(timeval::tv_usec)>(
              (time.count() % 1000) * 1000)};
}

// How long a read with BLOCK waits in Redis, where zero is for ever, or
// nothing without BLOCK.
std::optional<std::chrono::milliseconds> Blocks(
    const std::vector<std::string>& words) {
  const auto block = std::ranges::find(words, "BLOCK");
  int64_t wait = 0;
  if (block == words.end() || block + 1 == words.end() ||
      !absl::SimpleAtoi(*(block + 1), &wait)) {
    return std::nullopt;
  }
  return std::chrono::milliseconds(wait);
}

// A call, as a span under the request it's made for, named by its commands,
// such as GETDEL, or SREM HMGET for a pipeline of both, without their keys or
// values.
Span StartCall(std::span<const std::vector<std::string>> commands) {
  if (!InTrace()) {
    return Span();
  }
  std::vector<std::string_view> names;
  for (const std::vector<std::string>& words : commands) {
    if (!words.empty() && std::ranges::find(names, words[0]) == names.end()) {
      names.emplace_back(words[0]);
    }
  }
  const std::string operation = absl::StrJoin(names, " ");
  Span span =
      StartChildSpan(absl::StrCat("redis ", operation), SpanKind::kClient);
  span.SetAttribute("db.system", "redis");
  span.SetAttribute("db.operation", operation);
  return span;
}

void Append(redisContext* context, const std::vector<std::string>& words) {
  // Each word goes with its length, so it can hold any bytes, spaces and all.
  std::vector<const char*> argv;
  std::vector<std::size_t> lengths;
  for (const std::string& word : words) {
    argv.push_back(word.data());
    lengths.push_back(word.size());
  }
  redisAppendCommandArgv(context, static_cast<int>(argv.size()), argv.data(),
                         lengths.data());
}

std::string Text(const redisReply& reply) {
  if (reply.type == REDIS_REPLY_INTEGER) {
    return absl::StrCat(reply.integer);
  }
  return reply.str != nullptr ? std::string(reply.str, reply.len) : "";
}

// Each key in KEYS has to hold the value at its place in ARGV, or nothing
// runs, and it returns those that don't. Past those values, ARGV has the
// commands, each as how many words it has, and then its words.
constexpr char kIf[] = R"lua(
local wrong = {}
for i, key in ipairs(KEYS) do
  if redis.call('GET', key) ~= ARGV[i] then
    wrong[#wrong + 1] = key
  end
end
if #wrong > 0 then
  return wrong
end
local i = #KEYS + 1
while i <= #ARGV do
  local words = tonumber(ARGV[i])
  redis.call(unpack(ARGV, i + 1, i + words))
  i = i + words + 1
end
return wrong
)lua";

}  // namespace

absl::StatusOr<std::unique_ptr<Redis>> Redis::Create(const std::string& host,
                                                     int port,
                                                     RedisTimeouts timeouts) {
  // hiredis keeps its own copies of the timeouts, and connects with them
  // again after a lost connection.
  const timeval answer = TimevalOf(timeouts.answer);
  redisOptions options{};
  REDIS_OPTIONS_SET_TCP(&options, host.c_str(), port);
  options.connect_timeout = &answer;
  options.command_timeout = &answer;
  redisContext* context = redisConnectWithOptions(&options);
  if (context == nullptr) {
    return absl::ResourceExhaustedError("out of memory connecting to Redis");
  }
  // The object owns the context from here, so an error below frees it.
  std::unique_ptr<Redis> redis = absl::WrapUnique(new Redis(context, timeouts));
  if (context->err != 0) {
    return absl::UnavailableError(
        absl::StrCat("couldn't connect to Redis: ", context->errstr));
  }
  return redis;
}

Redis::~Redis() { redisFree(context_); }

absl::Status Redis::Ready() {
  // A lost connection, say from Redis restarting, or one that timed out,
  // leaves its error on the context, and hiredis can connect again with the
  // settings it kept. A connection that timed out is never used again, as a
  // reply that came late would answer the next command.
  if (context_->err != 0 && redisReconnect(context_) != REDIS_OK) {
    return absl::UnavailableError(
        absl::StrCat("couldn't reconnect to Redis: ", context_->errstr));
  }
  return absl::OkStatus();
}

absl::StatusOr<Reply> Redis::NextReply() {
  void* reply = nullptr;
  if (redisGetReply(context_, &reply) != REDIS_OK) {
    // A blocking socket whose wait for a reply runs out says to try again.
    const bool timed_out = context_->err == REDIS_ERR_TIMEOUT ||
                           (context_->err == REDIS_ERR_IO &&
                            (errno == EAGAIN || errno == EWOULDBLOCK));
    if (timed_out) {
      return absl::UnavailableError(absl::StrCat(
          "Redis didn't answer within ",
          absl::FormatDuration(absl::FromChrono(timeouts_.answer))));
    }
    return absl::UnavailableError(
        absl::StrCat("lost the connection to Redis: ", context_->errstr));
  }
  return Reply(static_cast<redisReply*>(reply), freeReplyObject);
}

absl::StatusOr<std::string> Redis::Command(
    const std::vector<std::string>& words) {
  const Span span = StartCall({&words, 1});
  const absl::Status ready = Ready();
  if (!ready.ok()) {
    return ready;
  }
  Append(context_, words);
  const absl::StatusOr<Reply> reply_or = NextReply();
  if (!reply_or.ok()) {
    return reply_or.status();
  }
  const Reply& reply = *reply_or;

  switch (reply->type) {
    case REDIS_REPLY_NIL:
      return absl::NotFoundError("Redis has nothing there");
    case REDIS_REPLY_INTEGER:
      return absl::StrCat(reply->integer);
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_STATUS:
      return std::string(reply->str, reply->len);
    case REDIS_REPLY_ERROR:
      return absl::InternalError(
          absl::StrCat("Redis: ", std::string_view(reply->str, reply->len)));
    default:
      return absl::UnimplementedError(
          "Redis replied with a type we don't read");
  }
}

absl::StatusOr<std::vector<Redis::Entry>> Redis::Read(
    const std::vector<std::string>& words) {
  const Span span = StartCall({&words, 1});
  const absl::Status ready = Ready();
  if (!ready.ok()) {
    return ready;
  }
  Append(context_, words);
  // A read that waits for entries to come waits that long for its reply, on
  // top of the timeout, and then the timeout's as it was. BLOCK 0 waits for
  // ever, and so does its read, as a timeout of zero is none.
  const std::optional<std::chrono::milliseconds> blocks = Blocks(words);
  if (blocks) {
    const std::chrono::milliseconds wait =
        blocks->count() == 0 ? *blocks : timeouts_.answer + *blocks;
    redisSetTimeout(context_, TimevalOf(wait));
  }
  const absl::StatusOr<Reply> reply = NextReply();
  // hiredis keeps the timeout for the connection it makes next, too, so it's
  // put back even after a reply that didn't come.
  if (blocks) {
    redisSetTimeout(context_, TimevalOf(timeouts_.answer));
  }
  if (!reply.ok()) {
    return reply.status();
  }
  if ((*reply)->type == REDIS_REPLY_ERROR) {
    return absl::InternalError(absl::StrCat("Redis: ", Text(**reply)));
  }
  // Nil when it timed out, or else a list of streams, each a list of its name
  // and its entries, each a list of its id and its fields.
  std::vector<Entry> entries;
  if ((*reply)->type != REDIS_REPLY_ARRAY || (*reply)->elements == 0) {
    return entries;
  }
  const redisReply& stream = *(*reply)->element[0];
  if (stream.type != REDIS_REPLY_ARRAY || stream.elements < 2 ||
      stream.element[1]->type != REDIS_REPLY_ARRAY) {
    return absl::InternalError("Redis: a stream we can't read");
  }
  const redisReply& read = *stream.element[1];
  for (std::size_t i = 0; i < read.elements; ++i) {
    const redisReply& entry = *read.element[i];
    if (entry.type != REDIS_REPLY_ARRAY || entry.elements < 2) {
      continue;
    }
    Entry& added = entries.emplace_back();
    added.id = Text(*entry.element[0]);
    // An entry trimmed away since it was read has nil for its fields.
    const redisReply& fields = *entry.element[1];
    for (std::size_t j = 0;
         fields.type == REDIS_REPLY_ARRAY && j < fields.elements; ++j) {
      added.fields.push_back(Text(*fields.element[j]));
    }
  }
  return entries;
}

absl::StatusOr<std::vector<std::vector<std::string>>> Redis::Pipeline(
    const std::vector<std::vector<std::string>>& commands) {
  const Span span = StartCall(commands);
  const absl::Status ready = Ready();
  if (!ready.ok()) {
    return ready;
  }
  for (const std::vector<std::string>& words : commands) {
    Append(context_, words);
  }
  std::vector<std::vector<std::string>> replies;
  absl::Status error;
  for (std::size_t i = 0; i < commands.size(); ++i) {
    const absl::StatusOr<Reply> reply = NextReply();
    if (!reply.ok()) {
      return reply.status();
    }
    if ((*reply)->type == REDIS_REPLY_ERROR) {
      error.Update(absl::InternalError(absl::StrCat("Redis: ", Text(**reply))));
      // We read on after an error, or the replies left over would come back
      // as the replies to the next commands.
      continue;
    }
    std::vector<std::string>& values = replies.emplace_back();
    if ((*reply)->type == REDIS_REPLY_ARRAY) {
      for (std::size_t j = 0; j < (*reply)->elements; ++j) {
        values.push_back(Text(*(*reply)->element[j]));
      }
    } else if ((*reply)->type != REDIS_REPLY_NIL) {
      values.push_back(Text(**reply));
    }
  }
  if (!error.ok()) {
    return error;
  }
  return replies;
}

absl::Status Redis::Transaction(
    const std::vector<std::vector<std::string>>& commands) {
  const Span span = StartCall(commands);
  const absl::Status ready = Ready();
  if (!ready.ok()) {
    return ready;
  }
  Append(context_, {"MULTI"});
  for (const std::vector<std::string>& words : commands) {
    Append(context_, words);
  }
  Append(context_, {"EXEC"});
  // MULTI's OK, and each command's QUEUED, or an error for one Redis
  // wouldn't queue, which makes EXEC run none of them.
  absl::Status error;
  for (std::size_t i = 0; i <= commands.size(); ++i) {
    const absl::StatusOr<Reply> reply = NextReply();
    if (!reply.ok()) {
      return reply.status();
    }
    if ((*reply)->type == REDIS_REPLY_ERROR) {
      error.Update(absl::InternalError(absl::StrCat("Redis: ", Text(**reply))));
    }
  }
  const absl::StatusOr<Reply> exec = NextReply();
  if (!exec.ok()) {
    return exec.status();
  }
  if (!error.ok()) {
    return error;
  }
  // EXEC's reply is a list of the commands' replies, once it's run them.
  // Redis doesn't undo the others when one fails as it runs, say on a key
  // that holds another type, so that's an error too.
  if ((*exec)->type != REDIS_REPLY_ARRAY ||
      (*exec)->elements != commands.size()) {
    return absl::InternalError(
        absl::StrCat("Redis didn't run the transaction: ", Text(**exec)));
  }
  for (std::size_t i = 0; i < (*exec)->elements; ++i) {
    if ((*exec)->element[i]->type == REDIS_REPLY_ERROR) {
      return absl::InternalError(
          absl::StrCat("Redis: ", Text(*(*exec)->element[i])));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<std::string>> Redis::TransactionIf(
    const std::map<std::string, std::string>& holding,
    const std::vector<std::vector<std::string>>& commands) {
  std::vector<std::string> eval = {"EVAL", kIf, absl::StrCat(holding.size())};
  for (const auto& [key, value] : holding) {
    eval.push_back(key);
  }
  for (const auto& [key, value] : holding) {
    eval.push_back(value);
  }
  for (const std::vector<std::string>& words : commands) {
    eval.push_back(absl::StrCat(words.size()));
    eval.insert(eval.end(), words.begin(), words.end());
  }
  absl::StatusOr<std::vector<std::vector<std::string>>> wrong =
      Pipeline({eval});
  if (!wrong.ok()) {
    return wrong.status();
  }
  return std::move(wrong->front());
}

}  // namespace nanommo
