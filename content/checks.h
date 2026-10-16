#ifndef NANOMMO_CONTENT_CHECKS_H_
#define NANOMMO_CONTENT_CHECKS_H_

#include <cstdint>
#include <map>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "terrain/lands.h"

namespace nanommo {

/// Items, or kinds of monster or NPC, by id, or what's wrong with an id: an
/// id of zero, which is what protobuf gives a field we leave out, or one that
/// two of them have.
template <typename Entry>
absl::StatusOr<std::map<uint32_t, Entry>> ById(
    const google::protobuf::RepeatedPtrField<Entry>& entries) {
  std::map<uint32_t, Entry> by_id;
  for (const Entry& entry : entries) {
    if (entry.id() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", entry.name(), " has no id"));
    }
    if (!by_id.emplace(entry.id(), entry).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("two of them have the id ", entry.id()));
    }
  }
  return by_id;
}

/// Whether a spawn names a kind we have, in a land we have.
template <typename Kind, typename Spawn>
absl::Status CheckSpawn(const std::map<uint32_t, Kind>& kinds,
                        const Spawn& spawn) {
  if (!kinds.contains(spawn.kind())) {
    return absl::InvalidArgumentError(
        absl::StrCat("there's no kind ", spawn.kind()));
  }
  if (!IsLand(spawn.land())) {
    return absl::InvalidArgumentError(
        absl::StrCat("there's no land called ", spawn.land()));
  }
  return absl::OkStatus();
}

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_CHECKS_H_
