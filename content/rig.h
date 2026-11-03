#ifndef NANOMMO_CONTENT_RIG_H_
#define NANOMMO_CONTENT_RIG_H_

#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "content/rig.pb.h"

namespace nanommo {

/// The rig in text format, or what's wrong with it: a field rig.proto doesn't
/// have, a joint with no name, or the name of another, a joint whose parent
/// isn't listed before it, a second joint without one, a build that moves a
/// joint the rig doesn't have, a walk or a run with no speed or no cycle, or
/// whose feet are down for none of it or all of it, or an idle or a death
/// whose keys go back in time.
absl::StatusOr<rig::Rig> ParseRig(std::string_view text);

/// The joints of one of the rig's builds: each mirrored one followed by its
/// twin on the right, so each still comes after its parent, and where the
/// build moves them.
std::vector<rig::Joint> Skeleton(const rig::Rig& rig, int build);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_RIG_H_
