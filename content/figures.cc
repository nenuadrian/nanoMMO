#include "content/figures.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "content/text.h"

namespace nanommo {

namespace {

// A bone on the left has Left in its name where its twin has Right.
std::string ToTheRight(std::string bone) {
  if (bone.starts_with("Left")) {
    bone.replace(0, 4, "Right");
  }
  return bone;
}

}  // namespace

absl::Status CheckPart(const figures::Part& part) {
  if (part.bone().empty()) {
    return absl::InvalidArgumentError("a part has no bone to move with");
  }
  const std::string& bone = part.bone();
  switch (part.shape()) {
    case figures::Part::SHAPE_LIMB:
      if (part.to().empty() || part.start_radius() <= 0.0f ||
          part.end_radius() <= 0.0f) {
        return absl::InvalidArgumentError(absl::StrCat(
            "a limb on the ", bone, " needs an end, and a radius at each"));
      }
      return absl::OkStatus();
    case figures::Part::SHAPE_BALL:
      if (part.size().x() <= 0.0f || part.size().y() <= 0.0f ||
          part.size().z() <= 0.0f) {
        return absl::InvalidArgumentError(
            absl::StrCat("a ball on the ", bone, " needs a size each way"));
      }
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("a part on the ", bone, " has no shape"));
  }
}

absl::StatusOr<figures::Figures> ParseFigures(std::string_view text) {
  figures::Figures file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  for (const figures::Part& part : file.body()) {
    const absl::Status checked = CheckPart(part);
    if (!checked.ok()) {
      return checked;
    }
  }
  for (const figures::Hair& hair : file.hairs()) {
    for (const figures::Part& part : hair.parts()) {
      const absl::Status checked = CheckPart(part);
      if (!checked.ok()) {
        return absl::InvalidArgumentError(
            absl::StrCat("a way to wear the hair: ", checked.message()));
      }
    }
  }
  return file;
}

Features FeaturesOf(const figures::Figures& figures, uint32_t id) {
  // The id times a large odd number scatters its bits, so that players who
  // join one after another don't look alike, and each feature picks with
  // the bits from a byte further up.
  const uint32_t mixed = id * 2654435761u;
  auto pick = [&](int count, int byte) {
    return static_cast<int>((mixed >> (8 * byte)) % count);
  };
  Features features;
  if (!figures.skins().empty()) {
    features.skin = figures.skins(pick(figures.skins_size(), 1));
  }
  if (!figures.hair_colors().empty()) {
    features.hair_color =
        figures.hair_colors(pick(figures.hair_colors_size(), 2));
  }
  if (!figures.hairs().empty()) {
    features.hair = pick(figures.hairs_size(), 3);
  }
  return features;
}

std::vector<figures::Part> Unfold(
    const google::protobuf::RepeatedPtrField<figures::Part>& parts) {
  std::vector<figures::Part> unfolded;
  for (const figures::Part& part : parts) {
    unfolded.push_back(part);
    if (!part.mirrored()) {
      continue;
    }
    figures::Part right = part;
    right.set_bone(ToTheRight(part.bone()));
    right.set_from(ToTheRight(part.from()));
    right.set_to(ToTheRight(part.to()));
    right.mutable_from_offset()->set_x(-part.from_offset().x());
    right.mutable_to_offset()->set_x(-part.to_offset().x());
    unfolded.push_back(right);
  }
  return unfolded;
}

}  // namespace nanommo
