#ifndef NANOMMO_CLIENT_THEME_H_
#define NANOMMO_CLIENT_THEME_H_

#include <imgui.h>

#include <string>

namespace nanommo {

/// nanoMMO's own colours, as its videos and its website have them: the near
/// black of a dying fire, its reds, its embers, gold for what's prized, and
/// the cream of old paper for words.
namespace colors {
inline constexpr ImU32 kNight = IM_COL32(11, 5, 7, 255);
inline constexpr ImU32 kCoal = IM_COL32(30, 14, 17, 255);
inline constexpr ImU32 kWine = IM_COL32(76, 11, 20, 255);
inline constexpr ImU32 kCrimson = IM_COL32(148, 18, 31, 255);
inline constexpr ImU32 kRed = IM_COL32(196, 29, 41, 255);
inline constexpr ImU32 kEmber = IM_COL32(255, 90, 60, 255);
inline constexpr ImU32 kFlame = IM_COL32(255, 160, 74, 255);
inline constexpr ImU32 kGold = IM_COL32(255, 211, 77, 255);
inline constexpr ImU32 kGoldLight = IM_COL32(255, 243, 196, 255);
inline constexpr ImU32 kGoldDark = IM_COL32(201, 138, 18, 255);
/// The trim round every panel: gold gone dark with age.
inline constexpr ImU32 kBrass = IM_COL32(138, 98, 38, 255);
inline constexpr ImU32 kPaper = IM_COL32(243, 233, 220, 255);
inline constexpr ImU32 kMuted = IM_COL32(181, 162, 151, 255);
inline constexpr ImU32 kFaint = IM_COL32(143, 123, 112, 255);
inline constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 200);
inline constexpr ImU32 kHealth = IM_COL32(214, 52, 46, 255);
inline constexpr ImU32 kHealthDark = IM_COL32(110, 16, 22, 255);
inline constexpr ImU32 kGood = IM_COL32(126, 214, 112, 255);
}  // namespace colors

/// The same colour, with its alpha times the share.
ImU32 Faded(ImU32 color, float share);

/// The faces the interface is set in: a book face for what's read, its bold,
/// and a carved capital face for titles. Before they're loaded, as in tests,
/// each is ImGui's own.
struct Fonts {
  ImFont* text = nullptr;
  ImFont* bold = nullptr;
  ImFont* title = nullptr;
};
const Fonts& GetFonts();

/// Loads the faces from the folder, in ImGui's font loading, with Font
/// Awesome's icons merged into the text face after them.
void LoadFonts(const std::string& folder);

/// Where the client's fonts are, next to it.
std::string FontFolder();

/// In points, at a scale of one.
inline constexpr float kTextSize = 18.0f;

/// How much bigger than on a screen 1080 points high the interface is drawn,
/// for a screen this high, times the player's own scale: twice as big on a
/// 4K screen, so that it's the same share of the screen on each, but never
/// so small that it can't be read.
float InterfaceScale(float height, float player);

/// The scale the theme was last applied at, which points in the interface's
/// layouts are multiplied by.
float UiScale();
inline float Ui(float points) { return points * UiScale(); }
inline ImVec2 Ui(ImVec2 points) { return {Ui(points.x), Ui(points.y)}; }

/// ImGui's style in our colours, with its sizes and its text at the scale.
void ApplyTheme(float scale);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_THEME_H_
