#include "protocol/places.h"

#include <cstdint>

namespace nanommo {

protocol::Place InBag(uint32_t slot) {
  protocol::Place place;
  place.set_bag(slot);
  return place;
}

protocol::Place Worn(items::Slot slot) {
  protocol::Place place;
  place.set_worn(slot);
  return place;
}

}  // namespace nanommo
