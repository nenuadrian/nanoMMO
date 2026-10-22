#include "server/consul.h"

#include <httplib.h>

#include <cstdint>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/udp.hpp"
#include "nlohmann/json.hpp"

namespace nanommo {

namespace {

constexpr char kService[] = "nanommo-world";

httplib::Client Connect(const std::string& host) {
  httplib::Client http(host, Consul::kPort);
  http.set_connection_timeout(1);
  http.set_read_timeout(1);
  return http;
}

absl::StatusOr<std::string> Answer(const httplib::Result& result) {
  if (!result) {
    return absl::UnavailableError(absl::StrCat(
        "couldn't reach Consul: ", httplib::to_string(result.error())));
  }
  if (result->status == 404) {
    return absl::NotFoundError(absl::StrCat("Consul: ", result->body));
  }
  if (result->status != 200) {
    return absl::InternalError(
        absl::StrCat("Consul answered ", result->status, ": ", result->body));
  }
  return result->body;
}

// nlohmann's json throws when a value isn't the type we ask for, so this
// checks first, and returns nullptr when it isn't there or isn't a T.
template <typename T>
const T* Find(const nlohmann::json& json, const char* path) {
  const nlohmann::json::json_pointer pointer(path);
  return json.contains(pointer) ? json.at(pointer).get_ptr<const T*>()
                                : nullptr;
}

}  // namespace

std::string InstanceId(const WorldRegistration& world) {
  return absl::StrCat(world.name, "-", world.port);
}

absl::Status Consul::Register(const WorldRegistration& world) const {
  const absl::StatusOr<std::string> address = OwnAddress();
  if (!address.ok()) {
    return address.status();
  }
  const nlohmann::json service = {
      {"ID", InstanceId(world)},
      {"Name", kService},
      {"Address", *address},
      {"Port", world.port},
      {"Meta",
       {{"world", world.name},
        {"started", absl::StrCat(world.started)},
        {"players", absl::StrCat(world.players)},
        {"metrics_port", absl::StrCat(world.metrics_port)},
        {"content", absl::StrCat(absl::Hex(world.content, absl::kZeroPad16))}}},
      {"TaggedAddresses",
       {{"wan", {{"Address", world.public_host}, {"Port", world.port}}}}},
      {"Check",
       {{"TTL", "10s"},
        {"Status", "passing"},
        {"DeregisterCriticalServiceAfter", "1m"}}},
  };
  return Put("/v1/agent/service/register", service.dump());
}

absl::Status Consul::Pass(const std::string& instance) const {
  return Put(absl::StrCat("/v1/agent/check/pass/service:", instance), "");
}

absl::Status Consul::Deregister(const std::string& instance) const {
  return Put(absl::StrCat("/v1/agent/service/deregister/", instance), "");
}

absl::StatusOr<std::vector<protocol::WorldListing>> Consul::Worlds() const {
  const absl::StatusOr<std::string> body =
      Get(absl::StrCat("/v1/health/service/", kService, "?passing"));
  if (!body.ok()) {
    return body.status();
  }
  const nlohmann::json entries = nlohmann::json::parse(*body, nullptr, false);
  if (!entries.is_array()) {
    return absl::DataLossError("Consul's list of worlds isn't a list");
  }
  // Each world's newest instance, with when it started, by the world's name.
  std::map<std::string, std::pair<int64_t, protocol::WorldListing>> newest;
  for (const nlohmann::json& entry : entries) {
    const auto* name = Find<std::string>(entry, "/Service/Meta/world");
    const auto* host =
        Find<std::string>(entry, "/Service/TaggedAddresses/wan/Address");
    const auto* port =
        Find<uint64_t>(entry, "/Service/TaggedAddresses/wan/Port");
    const auto* players = Find<std::string>(entry, "/Service/Meta/players");
    const auto* content = Find<std::string>(entry, "/Service/Meta/content");
    const auto* started = Find<std::string>(entry, "/Service/Meta/started");
    uint32_t count = 0;
    uint64_t hash = 0;
    int64_t since = 0;
    // Something under our name that isn't a world we could reach stays out,
    // so that the other worlds still make the list.
    if (!name || !host || !port || *port == 0 || *port > 65535 || !players ||
        !absl::SimpleAtoi(*players, &count) || !started ||
        !absl::SimpleAtoi(*started, &since)) {
      continue;
    }
    if (newest.contains(*name) && newest.at(*name).first > since) {
      continue;
    }
    protocol::WorldListing world;
    world.set_name(*name);
    world.set_host(*host);
    world.set_port(static_cast<uint32_t>(*port));
    world.set_players(count);
    // A world from before content hashes lists none, which no client has.
    if (content != nullptr && absl::SimpleHexAtoi(*content, &hash)) {
      world.set_content(hash);
    }
    newest[*name] = {since, std::move(world)};
  }
  // The map keeps the names in order.
  std::vector<protocol::WorldListing> worlds;
  worlds.reserve(newest.size());
  for (auto& [name, instance] : newest) {
    worlds.push_back(std::move(instance.second));
  }
  return worlds;
}

// The address Consul sees us at, which is the one the kernel picks to send to
// it from. Connecting a UDP socket sends nothing.
absl::StatusOr<std::string> Consul::OwnAddress() const {
  asio::io_context io;
  asio::ip::udp::resolver resolver(io);
  asio::ip::udp::socket socket(io);
  std::error_code error;
  const asio::ip::udp::resolver::results_type endpoints =
      resolver.resolve(host_, absl::StrCat(kPort), error);
  asio::ip::udp::endpoint own;
  if (!error) {
    asio::connect(socket, endpoints, error);
  }
  if (!error) {
    own = socket.local_endpoint(error);
  }
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't find Consul at ", host_, ": ", error.message()));
  }
  return own.address().to_string();
}

absl::Status Consul::Put(const std::string& path,
                         const std::string& body) const {
  return Answer(Connect(host_).Put(path, body, "application/json")).status();
}

absl::StatusOr<std::string> Consul::Get(const std::string& path) const {
  return Answer(Connect(host_).Get(path));
}

}  // namespace nanommo
