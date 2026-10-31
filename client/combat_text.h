#ifndef NANOMMO_CLIENT_COMBAT_TEXT_H_
#define NANOMMO_CLIENT_COMBAT_TEXT_H_

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

#include "client/animator.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A number or a line that rises over a unit as something happens to it, such
/// as the damage of a hit, and fades.
struct Popup {
  std::string text;
  /// The unit it rises over, by its id: a monster's, or else a player's.
  uint32_t unit = 0;
  bool monster = false;
  Color color = WHITE;
  /// What's about you is bigger than what's about the others.
  float scale = 1.0f;
  /// In seconds, since it came.
  float age = 0.0f;
};

/// What the events show, kept apart from the drawing, so that it can be
/// tested without a window.
class CombatText {
 public:
  /// In seconds.
  static constexpr float kLife = 1.5f;

  /// Adds what the event shows, to the player whose id is you.
  void Add(const protocol::Event& event, uint32_t you);

  /// Ages each by the frame's time, and drops the ones past their life.
  void Update(float seconds);

  const std::vector<Popup>& popups() const { return popups_; }

 private:
  std::vector<Popup> popups_;
};

/// Draws each over the figure it's about, rising and fading as it ages.
void DrawCombatText(const CombatText& text, const std::vector<Figure>& figures,
                    const Camera3D& camera);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_COMBAT_TEXT_H_
