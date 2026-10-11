#include "server/redis.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

// A lost connection, say from Redis restarting, leaves its error on the
// context, and hiredis can connect again with the settings it kept.
absl::Status ReconnectIfLost(redisContext* context) {
  if (context->err != 0 && redisReconnect(context) != REDIS_OK) {
    return absl::UnavailableError(
        absl::StrCat("couldn't reconnect to Redis: ", context->errstr));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<Redis>> Redis::Create(const std::string& host,
                                                     int port) {
  redisContext* context = redisConnect(host.c_str(), port);
  if (context == nullptr) {
    return absl::ResourceExhaustedError("out of memory connecting to Redis");
  }
  // The object owns the context from here, so an error below frees it.
  std::unique_ptr<Redis> redis = absl::WrapUnique(new Redis(context));
  if (context->err != 0) {
    return absl::UnavailableError(
        absl::StrCat("couldn't connect to Redis: ", context->errstr));
  }
  return redis;
}

Redis::~Redis() { redisFree(context_); }

absl::StatusOr<std::string> Redis::Command(
    const std::vector<std::string>& words) {
  const absl::Status connected = ReconnectIfLost(context_);
  if (!connected.ok()) {
    return connected;
  }
  // Each word goes with its length, so it can hold any bytes, spaces and all.
  std::vector<const char*> argv;
  std::vector<std::size_t> lengths;
  for (const std::string& word : words) {
    argv.push_back(word.data());
    lengths.push_back(word.size());
  }
  const std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply(
      static_cast<redisReply*>(redisCommandArgv(context_,
                                                static_cast<int>(argv.size()),
                                                argv.data(), lengths.data())),
      freeReplyObject);
  if (!reply) {
    return absl::UnavailableError(
        absl::StrCat("lost the connection to Redis: ", context_->errstr));
  }

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

}  // namespace nanommo
