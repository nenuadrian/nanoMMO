#ifndef NANOMMO_CLIENT_LOADING_H_
#define NANOMMO_CLIENT_LOADING_H_

#include <array>
#include <string>

namespace nanommo {

/// When a screen covers the world, while the world isn't there yet, kept apart
/// from raylib and ImGui so that it can be tested without a window. It covers
/// the world from the moment we enter one, or go into a new land, until the
/// land is built. Then it fades, and the fog lifts off the land, nearest
/// first, so that the land comes out of its sky instead of appearing at once.
class Loading {
 public:
  /// In seconds.
  static constexpr float kFade = 0.4f;
  static constexpr float kLift = 1.5f;

  /// Covers the world from the next frame on.
  void Cover();

  /// Whether the last frame drawn was covered, so that a land built now,
  /// which holds up the next frame, is built behind the cover.
  bool covered() const { return stage_ == Stage::kCovered; }

  /// Whether the cover's up, or going up.
  bool covering() const {
    return stage_ == Stage::kCovering || stage_ == Stage::kCovered;
  }

  /// The land is built, so the cover fades and the fog lifts.
  void Lift();

  /// Moves on by the frame's time, once the frame is drawn.
  void Update(float seconds);

  /// How much of the screen the cover hides, from 0 to 1.
  float cover() const;

  /// How far the fog has lifted, from 0, where it hides all but what's
  /// nearest, to 1, the land's own fog.
  float clear() const;

 private:
  enum class Stage { kNone, kCovering, kCovered, kLifting };

  Stage stage_ = Stage::kNone;
  float time_ = 0.0f;
};

/// Draws the cover over everything else, as much of it as cover says, in the
/// sky's colour, with the title in the middle and a line under it.
void DrawLoadingScreen(float cover, const std::array<float, 3>& sky,
                       const std::string& title, const std::string& line);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LOADING_H_
