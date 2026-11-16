#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "asio/io_context.hpp"
#include "asio/signal_set.hpp"
#include "asio/ssl/context.hpp"
#include "gateway/gateway.h"
#include "gateway/world_list.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "protocol/version.h"
#include "server/consul.h"

// The gateway between browsers' pages and the servers: WebSocket on its port,
// TLS to the login server and to the worlds Consul lists. In a deployment, a
// reverse proxy with a certificate browsers trust puts wss:// in front of it.
int main() {
  std::cout << "nanommo gateway: version " << nanommo::kVersion << std::endl;
  uint32_t port = 0;
  if (!absl::SimpleAtoi(nanommo::Setting("NANOMMO_GATEWAY_PORT", "8090"),
                        &port) ||
      port > 65535) {
    std::cerr << "nanommo gateway: NANOMMO_GATEWAY_PORT isn't a port\n";
    return 1;
  }
  // The servers' certificates, checked as a client on a computer checks them.
  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(nanommo::FileSetting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    std::cerr << "nanommo gateway: " << tls.status() << "\n";
    return 1;
  }
  const std::string login = nanommo::Setting("NANOMMO_LOGIN_HOST", "localhost");
  const nanommo::Consul consul(
      nanommo::Setting("NANOMMO_CONSUL_HOST", "127.0.0.1"));

  asio::io_context io;
  // The worlds a page may reach, from Consul, asked on a thread of their own.
  nanommo::WorldList worlds(
      io, [&consul]() -> absl::StatusOr<std::vector<nanommo::ListedWorld>> {
        absl::StatusOr<std::vector<nanommo::WorldInstance>> instances =
            consul.Instances();
        if (!instances.ok()) {
          return instances.status();
        }
        std::vector<nanommo::ListedWorld> listed;
        for (nanommo::WorldInstance& instance : *instances) {
          listed.push_back({.public_host = std::move(instance.public_host),
                            .port = instance.port,
                            .address = std::move(instance.address)});
        }
        return listed;
      });
  worlds.Start();
  absl::StatusOr<std::unique_ptr<nanommo::Gateway>> gateway =
      nanommo::Gateway::Create(
          io, static_cast<uint16_t>(port), *tls,
          nanommo::Routes(
              {.host = login, .port = 7778, .name = login},
              [&worlds](std::string_view host, uint16_t world_port) {
                return worlds.Find(host, world_port,
                                   nanommo::WorldList::Clock::now());
              }));
  if (!gateway.ok()) {
    std::cerr << "nanommo gateway: " << gateway.status() << "\n";
    return 1;
  }
  std::cout << "nanommo gateway: listening on " << (*gateway)->port()
            << ", for the login server at " << login << std::endl;

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo gateway: stopped" << std::endl;
  return 0;
}
