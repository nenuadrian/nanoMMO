#include "gateway/world_list.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/post.hpp"

namespace nanommo {

WorldList::WorldList(asio::io_context& io, Fetch fetch)
    : io_(io), fetch_(std::move(fetch)), timer_(io) {}

WorldList::~WorldList() {
  timer_.cancel();
  thread_.join();
}

void WorldList::Start() {
  Ask();
  timer_.expires_after(std::chrono::seconds(1));
  timer_.async_wait([this](std::error_code error) {
    if (!error) {
      Start();
    }
  });
}

void WorldList::Ask() {
  // One question at a time: a slow Consul isn't asked again until it answers.
  if (asking_) {
    return;
  }
  asking_ = true;
  asio::post(thread_, [this] {
    absl::StatusOr<std::vector<ListedWorld>> worlds = fetch_();
    asio::post(io_, [this, worlds = std::move(worlds)]() mutable {
      asking_ = false;
      Take(std::move(worlds), Clock::now());
    });
  });
}

void WorldList::Take(absl::StatusOr<std::vector<ListedWorld>> worlds,
                     Clock::time_point at) {
  if (!worlds.ok()) {
    std::cerr << "nanommo gateway: couldn't list the worlds: "
              << worlds.status() << "\n";
    return;
  }
  worlds_ = *std::move(worlds);
  fetched_ = at;
}

absl::StatusOr<std::string> WorldList::Find(std::string_view public_host,
                                            uint16_t port,
                                            Clock::time_point now) const {
  if (!fetched_ || now - *fetched_ > kStale) {
    return absl::UnavailableError("the gateway has no recent list of worlds");
  }
  for (const ListedWorld& world : worlds_) {
    if (world.public_host == public_host && world.port == port) {
      return world.address;
    }
  }
  return absl::NotFoundError(
      absl::StrCat("no world at ", public_host, ":", port));
}

}  // namespace nanommo
