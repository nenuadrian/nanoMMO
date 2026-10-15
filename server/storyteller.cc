#include "server/storyteller.h"

#include <httplib.h>

#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "asio/post.hpp"
#include "nlohmann/json.hpp"

namespace nanommo {

namespace {

// The storyteller writes the stories that wait at the same time together, up
// to sixteen, and each of these threads waits for one story, so with fewer, it
// would write fewer at once.
constexpr int kThreads = 16;

// A story of a hundred tokens is a few lines of chat.
constexpr int kMaxTokens = 100;

// A story takes a second or two, and longer while the storyteller is busy.
// After ten, the player has stopped waiting.
constexpr int kTimeoutSeconds = 10;

}  // namespace

Storyteller::Storyteller(asio::io_context& io, std::string host)
    : io_(io), host_(std::move(host)), threads_(kThreads) {}

void Storyteller::Tell(std::string prompt, Told told) {
  asio::post(threads_, [this, prompt = std::move(prompt),
                        told = std::move(told)]() mutable {
    absl::StatusOr<std::string> story = Complete(prompt);
    asio::post(io_, [told = std::move(told), story = std::move(story)] {
      told(story);
    });
  });
}

absl::StatusOr<std::string> Storyteller::Complete(
    const std::string& prompt) const {
  httplib::Client http(host_, kPort);
  http.set_connection_timeout(1);
  http.set_read_timeout(kTimeoutSeconds);
  const nlohmann::json request = {{"prompt", prompt},
                                  {"max_tokens", kMaxTokens}};
  const httplib::Result result =
      http.Post("/v1/completions", request.dump(), "application/json");
  if (!result) {
    return absl::UnavailableError(
        absl::StrCat("couldn't reach the storyteller: ",
                     httplib::to_string(result.error())));
  }
  if (result->status != 200) {
    return absl::InternalError(absl::StrCat(
        "the storyteller answered ", result->status, ": ", result->body));
  }
  const nlohmann::json reply =
      nlohmann::json::parse(result->body, nullptr, false);
  const nlohmann::json::json_pointer text("/choices/0/text");
  if (!reply.is_object() || !reply.contains(text) ||
      !reply.at(text).is_string()) {
    return absl::DataLossError("the storyteller's answer has no story in it");
  }
  return reply.at(text).get<std::string>();
}

}  // namespace nanommo
