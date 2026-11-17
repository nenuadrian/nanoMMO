#ifndef NANOMMO_CLIENT_DAYLIGHT_H_
#define NANOMMO_CLIENT_DAYLIGHT_H_

#include "client/atmosphere.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Towards the sun at that time of day: up from the east at 0.25, overhead,
/// though a little to the south, at noon, and down in the west at 0.75.
std::array<float, 3> SunAt(float day);

/// What turns on and off: the time of day, and the weather.
struct Showing {
  bool day_night = true;
  bool weather = true;
};

/// The land's atmosphere under the sky the server sent: its day, dawn, dusk
/// or night, with the sun or the moon where the time puts it, and darker,
/// greyer and nearer under the weather. A land without days, the Crypt, keeps
/// its own night, and takes only the weather. Without the time of day, it's
/// the land's own look.
Atmosphere AtmosphereUnder(int land, const protocol::Sky& sky,
                           const Showing& showing);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_DAYLIGHT_H_
