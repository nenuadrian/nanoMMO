#ifndef NANOMMO_CLIENT_COMPASS_H_
#define NANOMMO_CLIENT_COMPASS_H_

#include <imgui.h>
#include <raylib.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "client/animator.h"

namespace nanommo {

/// Which way the camera looks, as a compass has it: in radians, clockwise from
/// north, which is up the map, along minus z, so that east, along plus x, is a
/// quarter turn.
float Heading(const Camera3D& camera);

/// Which way a place is from another, the same way.
float Bearing(Vector3 from, Vector3 to);

/// Where a bearing is across the compass, while the camera looks along the
/// heading: from -1 at the compass's left end to 1 at its right, with the
/// heading in the middle, or none when it's further round than half the span.
std::optional<float> CompassAt(float heading, float bearing, float span);

/// Something the compass shows the way to.
struct CompassMark {
  enum class Kind { kTarget, kQuest, kTurnIn, kGate, kDestination };
  Kind kind = Kind::kTarget;
  float bearing = 0.0f;
  /// In metres.
  float distance = 0.0f;
  ImU32 color = IM_COL32_WHITE;
};

/// What the compass shows the way to, from you: the monster you picked, the
/// NPCs with a quest for you, the land's gates, each in the sky of the land it
/// goes to, and where you're walking to.
std::vector<CompassMark> CompassMarks(const std::vector<Figure>& figures,
                                      int land, uint32_t target,
                                      std::optional<Vector3> destination);

/// The compass along the top of the screen: the half of the world in front of
/// the camera, with its points, and the marks on it. The monster you picked
/// and the quests stay at its ends while they're behind you, so that you
/// know which way to turn.
void DrawCompass(float heading, const std::vector<CompassMark>& marks);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_COMPASS_H_
