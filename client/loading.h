#ifndef NANOMMO_CLIENT_LOADING_H_
#define NANOMMO_CLIENT_LOADING_H_

#include <array>
#include <string>

namespace nanommo {

/// When a screen covers the world, while the world isn't there yet, kept apart
/// from raylib and ImGui so that it can be tested without a window. It covers
/// the world from the moment we enter one, or go into a new land, until the
/// land is built, and then fades.
class Loading {
 public:
  /// In seconds.
  static constexpr float kFade = 0.4f;

  /// Covers the world from the next frame on.
  void Cover();

  /// Whether the last frame drawn was covered, so that a land built now,
  /// which holds up the next frame, is built behind the cover.
  bool covered() const { return stage_ == Stage::kCovered; }

  /// Whether the cover's up, or going up.
  bool covering() const {
    return stage_ == Stage::kCovering || stage_ == Stage::kCovered;
  }

  /// The land is built, so the cover fades.
  void Lift();

  /// Moves on by the frame's time, once the frame is drawn.
  void Update(float seconds);

  /// How much of the screen the cover hides, from 0 to 1.
  float cover() const;

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
