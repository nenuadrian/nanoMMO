#include "server/consul.h"

#include <httplib.h>

#include <string>
#include <system_error>

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

}  // namespace

absl::Status Consul::Register(const WorldRegistration& world) const {
  const absl::StatusOr<std::string> address = OwnAddress();
  if (!address.ok()) {
    return address.status();
  }
  const nlohmann::json service = {
      {"ID", world.name},
      {"Name", kService},
      {"Address", *address},
      {"Port", world.port},
      {"Meta",
       {{"players", absl::StrCat(world.players)},
        {"metrics_port", absl::StrCat(world.metrics_port)}}},
      {"TaggedAddresses",
       {{"wan", {{"Address", world.public_host}, {"Port", world.port}}}}},
      {"Check",
       {{"TTL", "10s"},
        {"Status", "passing"},
        {"DeregisterCriticalServiceAfter", "1m"}}},
  };
  return Put("/v1/agent/service/register", service.dump());
}

absl::Status Consul::Pass(const std::string& name) const {
  return Put(absl::StrCat("/v1/agent/check/pass/service:", name), "");
}

absl::Status Consul::Deregister(const std::string& name) const {
  return Put(absl::StrCat("/v1/agent/service/deregister/", name), "");
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

}  // namespace nanommo
