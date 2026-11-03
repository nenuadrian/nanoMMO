#include "content/rig.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "content/text.h"

namespace nanommo {

namespace {

// A joint on the left has Left in its name where its twin has Right.
std::string ToTheRight(std::string name) {
  if (name.starts_with("Left")) {
    name.replace(0, 4, "Right");
  }
  return name;
}

absl::Status CheckGait(const rig::Gait& gait, std::string_view name) {
  if (gait.speed() <= 0.0f || gait.cycle_ms() == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("the ", name, " needs a speed and a cycle"));
  }
  if (gait.stance() <= 0.0f || gait.stance() >= 1.0f) {
    return absl::InvalidArgumentError(absl::StrCat(
        "the ", name, "'s feet are down for none of its cycle, or all of it"));
  }
  return absl::OkStatus();
}

absl::Status CheckMove(const skills::Move& move, std::string_view name) {
  uint32_t last = 0;
  for (const skills::Key& key : move.keys()) {
    if (key.at_ms() < last) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", name, " goes back in time"));
    }
    last = key.at_ms();
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<rig::Rig> ParseRig(std::string_view text) {
  rig::Rig file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  // The joints so far, by name, with the twins on the right.
  std::set<std::string> joints;
  for (const rig::Joint& joint : file.joints()) {
    if (joint.name().empty()) {
      return absl::InvalidArgumentError("a joint has no name");
    }
    if (joint.parent().empty() ? !joints.empty()
                               : !joints.contains(joint.parent())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "the ", joint.name(), " hangs from a joint that isn't before it"));
    }
    std::vector<std::string> names = {joint.name()};
    if (joint.mirrored()) {
      names.push_back(ToTheRight(joint.name()));
    }
    for (const std::string& name : names) {
      if (!joints.insert(name).second) {
        return absl::InvalidArgumentError(
            absl::StrCat("there are two joints called ", name));
      }
    }
  }
  for (const rig::Build& build : file.builds()) {
    for (const rig::Shift& shift : build.shifts()) {
      if (!joints.contains(shift.joint())) {
        return absl::InvalidArgumentError(absl::StrCat(
            "a build moves a joint ", shift.joint(), " the rig doesn't have"));
      }
    }
  }
  absl::Status checked = CheckGait(file.walk(), "walk");
  if (checked.ok()) {
    checked = CheckGait(file.run(), "run");
  }
  if (checked.ok()) {
    checked = CheckMove(file.idle(), "idle");
  }
  if (checked.ok()) {
    checked = CheckMove(file.death(), "death");
  }
  if (!checked.ok()) {
    return checked;
  }
  return file;
}

std::vector<rig::Joint> Skeleton(const rig::Rig& rig, int build) {
  std::vector<rig::Joint> joints;
  for (const rig::Joint& joint : rig.joints()) {
    joints.push_back(joint);
    if (!joint.mirrored()) {
      continue;
    }
    rig::Joint right = joint;
    right.set_name(ToTheRight(joint.name()));
    right.set_parent(ToTheRight(joint.parent()));
    right.mutable_at()->set_x(-joint.at().x());
    joints.push_back(right);
  }
  if (build < 0 || build >= rig.builds_size()) {
    return joints;
  }
  for (const rig::Shift& shift : rig.builds(build).shifts()) {
    // The joint, and the joints that hang from it, which come after it, as a
    // joint comes after its parent. Its twin moves the other way across.
    const std::string twin = ToTheRight(shift.joint());
    for (const bool right : {false, true}) {
      if (right && twin == shift.joint()) {
        continue;
      }
      std::set<std::string> moved = {right ? twin : shift.joint()};
      for (rig::Joint& joint : joints) {
        if (!moved.contains(joint.name()) && !moved.contains(joint.parent())) {
          continue;
        }
        moved.insert(joint.name());
        figures::Offset& at = *joint.mutable_at();
        at.set_x(at.x() + (right ? -shift.by().x() : shift.by().x()));
        at.set_y(at.y() + shift.by().y());
        at.set_z(at.z() + shift.by().z());
      }
    }
  }
  return joints;
}

}  // namespace nanommo
