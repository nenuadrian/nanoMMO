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

/// A panel's shape: a rectangle with its corners cut off.
void PathPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, float cut);

/// A panel, as every window has: dark, in a shape with its corners cut, a red
/// glow under its title, trimmed in brass with a thin light inside it, and a
/// gem at its top corners.
void DrawPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 0.96f);

/// A diamond, filled, with an outline.
void DrawDiamond(ImDrawList* draw, ImVec2 centre, float radius, ImU32 fill,
                 ImU32 outline);

/// A line that fades in from its ends, with a diamond in its middle, under a
/// title.
void DrawDivider(ImDrawList* draw, ImVec2 left, float width);

/// Text with a shadow under it.
void DrawShadowedText(ImDrawList* draw, ImFont* font, float size, ImVec2 at,
                      ImU32 color, const char* text);

/// Text that goes from one colour at its top to another at its bottom, with
/// a shadow, as titles have in gold.
void DrawGradientText(ImDrawList* draw, ImFont* font, float size, ImVec2 at,
                      ImU32 top, ImU32 bottom, const char* text);

/// A title in carved gold capitals, centred on the point at its top.
void DrawTitle(ImDrawList* draw, ImVec2 top_centre, float size,
               const char* text);

/// The part of an ImGui label that's shown: what's before any "##".
std::string Shown(const char* label);

/// Begins a window drawn as one of our panels, with its title at the top and a
/// close button that sets open to false, if open isn't null. Always call
/// EndPanel after it, as ImGui's End.
bool BeginPanel(const char* name, bool* open, ImGuiWindowFlags flags = 0,
                float alpha = 0.96f);
void EndPanel();

/// A button with its corners cut, in a dark red, or, for what a screen is
/// there for, such as entering the world, in the colours of a fire. Returns
/// whether it was clicked. A width of zero fits the label, and one below zero
/// takes the rest of the line.
bool GameButton(const char* label, ImVec2 size, bool primary = false,
                bool enabled = true);

/// A bar that's full by the share, in its colour, in a dark trough with brass
/// round it and its words in the middle.
void DrawBar(ImDrawList* draw, ImVec2 min, ImVec2 max, float share, ImU32 color,
             ImU32 dark, const char* text);

/// A slot, as the bag's and the hotbar's are: a dark well in brass, which a
/// colour lights round its edge, such as gold while the mouse is over it, or
/// green or red while something dragged over it would go there or not.
void DrawSlot(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 edge);

/// An icon in the middle of a slot, in its colour, on a glow of that colour.
void DrawSlotIcon(ImDrawList* draw, ImVec2 min, ImVec2 max, const char* icon,
                  ImU32 color, float alpha = 1.0f);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_THEME_H_
