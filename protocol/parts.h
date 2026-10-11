#ifndef NANOMMO_PROTOCOL_PARTS_H_
#define NANOMMO_PROTOCOL_PARTS_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Most networks carry 1500 bytes a packet, less what tunnels and VPNs
/// take, so IP never has to split a datagram this size.
constexpr std::size_t kMaxDatagramSize = 1200;

std::vector<protocol::World> Split(const protocol::World& world,
                                   std::size_t max_size = kMaxDatagramSize);

class Parts {
 public:
  void Add(const protocol::World& part);

  protocol::World world() const;

 private:
  uint32_t you_ = 0;
  int64_t sent_ = 0;
  std::map<uint32_t, protocol::Player> players_;
  std::set<uint32_t> arrived_;
  std::set<uint32_t> seen_;
};

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_PARTS_H_
