#ifndef NANOMMO_SERVER_CONSUL_H_
#define NANOMMO_SERVER_CONSUL_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A world, as it registers with Consul.
struct WorldRegistration {
  std::string name;
  std::string public_host;
  uint16_t port = 0;
  uint16_t metrics_port = 0;
  int players = 0;
  /// The ClientHash of the world's content.
  uint64_t content = 0;
};

class Consul {
 public:
  static constexpr uint16_t kPort = 8500;

  explicit Consul(std::string host) : host_(std::move(host)) {}

  /// Registers the world, or registers it again with its players. Its health
  /// check starts as passing, and keeps its status when it's registered again.
  absl::Status Register(const WorldRegistration& world) const;

  /// Passes the world's health check, which turns critical ten seconds after
  /// the last pass. NotFound when Consul doesn't know the world.
  absl::Status Pass(const std::string& name) const;

  absl::Status Deregister(const std::string& name) const;

  /// The worlds that pass their health checks, by name, with the host and
  /// port players reach them at.
  absl::StatusOr<std::vector<protocol::WorldListing>> Worlds() const;

 private:
  absl::StatusOr<std::string> OwnAddress() const;
  absl::Status Put(const std::string& path, const std::string& body) const;
  absl::StatusOr<std::string> Get(const std::string& path) const;

  std::string host_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_CONSUL_H_
