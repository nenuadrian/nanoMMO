#ifndef NANOMMO_PROTOCOL_PLACES_H_
#define NANOMMO_PROTOCOL_PLACES_H_

#include <cstdint>

#include "content/items.pb.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A place in the bag, by its slot from zero.
protocol::Place InBag(uint32_t slot);

/// A slot that's worn.
protocol::Place Worn(items::Slot slot);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_PLACES_H_
