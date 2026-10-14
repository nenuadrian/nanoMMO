#ifndef NANOMMO_PROTOCOL_PARTS_H_
#define NANOMMO_PROTOCOL_PARTS_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Most networks carry 1500 bytes a packet, less what tunnels and VPNs
/// take, so IP never has to split a datagram this size.
constexpr std::size_t kMaxDatagramSize = 1200;

/// A player or a monster in a world of its own, serialized. Protobuf reads
/// worlds that follow each other as one, so pieces put one after another make
/// a world.
std::string Piece(const protocol::Player& player);
std::string Piece(const protocol::Monster& monster);

/// The parts of a world sent at a time, each a datagram of up to max_size
/// bytes, packed from pieces.
std::vector<std::string> Split(const std::vector<std::string_view>& pieces,
                               int64_t sent,
                               std::size_t max_size = kMaxDatagramSize);

class Parts {
 public:
  void Add(const protocol::World& part);

  protocol::World world() const;

 private:
  uint32_t you_ = 0;
  uint32_t experience_ = 0;
  uint32_t next_level_ = 0;
  uint32_t seed_ = 0;
  int64_t sent_ = 0;
  std::map<uint32_t, protocol::Player> players_;
  std::map<uint32_t, protocol::Monster> monsters_;
  std::set<uint32_t> arrived_;
  std::set<uint32_t> seen_;
  std::set<uint32_t> seen_monsters_;
};

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_PARTS_H_
