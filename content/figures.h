#ifndef NANOMMO_CONTENT_FIGURES_H_
#define NANOMMO_CONTENT_FIGURES_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/figures.pb.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace nanommo {

/// What's wrong with a part, if anything: no shape, no bone to move with, a
/// limb with no end or no thickness, or a ball with no size.
absl::Status CheckPart(const figures::Part& part);

/// The figures in text format, or what's wrong with them: a field
/// figures.proto doesn't have, or a part of the body or of a way to wear the
/// hair that's wrong.
absl::StatusOr<figures::Figures> ParseFigures(std::string_view text);

/// What a player looks like under what they wear, which their id picks from
/// the figures' lists, so that every client draws them the same.
struct Features {
  /// As 0xRRGGBBAA: white, when there are none to pick from.
  uint32_t skin = 0xFFFFFFFF;
  uint32_t hair_color = 0xFFFFFFFF;
  /// Which way they wear their hair, from the figures' hairs, or -1 when
  /// there are none.
  int hair = -1;
};
Features FeaturesOf(const figures::Figures& figures, uint32_t id);

/// The parts, each mirrored one followed by its twin on the right, on the
/// right's bones, with its offsets the other way across.
std::vector<figures::Part> Unfold(
    const google::protobuf::RepeatedPtrField<figures::Part>& parts);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_FIGURES_H_
