#include "client/theme.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <string>

namespace nanommo {

namespace {

Fonts fonts;
float scale = 1.0f;

ImVec4 Float4(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }

ImVec4 Float4(ImU32 color, float alpha) {
  ImVec4 out = Float4(color);
  out.w = alpha;
  return out;
}

// One of the fonts in the folder, or null when it isn't there.
ImFont* AddFont(const std::string& folder, const char* file) {
  const std::string path = folder + "/" + file;
  if (!FileExists(path.c_str())) {
    return nullptr;
  }
  ImFontConfig config;
  config.PixelSnapH = true;
  return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), kTextSize,
                                                  &config);
}

// ImGui's own, for a face that isn't there, so that a client without its
// fonts still has words.
ImFont* OrDefault(ImFont* font) {
  if (font != nullptr) {
    return font;
  }
  ImFontConfig config;
  config.SizePixels = kTextSize;
  return ImGui::GetIO().Fonts->AddFontDefaultVector(&config);
}

}  // namespace

ImU32 Faded(ImU32 color, float share) {
  const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) *
                      std::clamp(share, 0.0f, 1.0f);
  return (color & ~IM_COL32_A_MASK) |
         (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

const Fonts& GetFonts() {
  static Fonts fallback;
  if (fonts.text == nullptr) {
    fallback.text = fallback.bold = fallback.title = ImGui::GetFont();
    return fallback;
  }
  return fonts;
}

void LoadFonts(const std::string& folder) {
  fonts.bold = OrDefault(AddFont(folder, "AlegreyaSans-Bold.ttf"));
  fonts.title = OrDefault(AddFont(folder, "Cinzel-Bold.ttf"));
  // Last, as rlImGui merges Font Awesome's icons into the last font added.
  fonts.text = OrDefault(AddFont(folder, "AlegreyaSans-Medium.ttf"));
  ImGui::GetIO().FontDefault = fonts.text;
}

std::string FontFolder() {
  return std::string(GetApplicationDirectory()) + "fonts";
}

float InterfaceScale(float height, float player) {
  constexpr float kReference = 1080.0f;
  constexpr float kSmallest = 0.75f;
  const float screen = std::max(height / kReference, kSmallest);
  return std::clamp(screen * std::clamp(player, 0.5f, 2.0f), 0.5f, 4.0f);
}

float UiScale() { return scale; }

void ApplyTheme(float at) {
  ImGuiStyle style;
  style.WindowPadding = {16.0f, 12.0f};
  style.FramePadding = {10.0f, 6.0f};
  style.ItemSpacing = {8.0f, 7.0f};
  style.ItemInnerSpacing = {6.0f, 5.0f};
  style.CellPadding = {6.0f, 4.0f};
  style.IndentSpacing = 18.0f;
  style.ScrollbarSize = 10.0f;
  style.GrabMinSize = 12.0f;
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 2.0f;
  style.PopupRounding = 2.0f;
  style.ScrollbarRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.TabRounding = 2.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.WindowTitleAlign = {0.5f, 0.5f};
  style.SeparatorTextAlign = {0.5f, 0.5f};
  style.SeparatorTextBorderSize = 1.0f;

  ImVec4* c = style.Colors;
  c[ImGuiCol_Text] = Float4(colors::kPaper);
  c[ImGuiCol_TextDisabled] = Float4(colors::kMuted);
  c[ImGuiCol_WindowBg] = Float4(colors::kNight, 0.94f);
  c[ImGuiCol_ChildBg] = Float4(colors::kNight, 0.0f);
  c[ImGuiCol_PopupBg] = Float4(IM_COL32(22, 10, 12, 255), 0.97f);
  c[ImGuiCol_Border] = Float4(colors::kBrass, 0.8f);
  c[ImGuiCol_BorderShadow] = Float4(colors::kNight, 0.0f);
  c[ImGuiCol_FrameBg] = Float4(colors::kNight, 0.85f);
  c[ImGuiCol_FrameBgHovered] = Float4(IM_COL32(40, 10, 15, 255));
  c[ImGuiCol_FrameBgActive] = Float4(IM_COL32(56, 12, 19, 255));
  c[ImGuiCol_TitleBg] = Float4(colors::kWine);
  c[ImGuiCol_TitleBgActive] = Float4(colors::kCrimson);
  c[ImGuiCol_TitleBgCollapsed] = Float4(colors::kWine, 0.6f);
  c[ImGuiCol_MenuBarBg] = Float4(colors::kCoal);
  c[ImGuiCol_ScrollbarBg] = Float4(colors::kNight, 0.0f);
  c[ImGuiCol_ScrollbarGrab] = Float4(colors::kWine);
  c[ImGuiCol_ScrollbarGrabHovered] = Float4(colors::kCrimson);
  c[ImGuiCol_ScrollbarGrabActive] = Float4(colors::kRed);
  c[ImGuiCol_CheckMark] = Float4(colors::kGold);
  c[ImGuiCol_CheckboxSelectedBg] = Float4(colors::kWine);
  c[ImGuiCol_SliderGrab] = Float4(colors::kGoldDark);
  c[ImGuiCol_SliderGrabActive] = Float4(colors::kGold);
  c[ImGuiCol_Button] = Float4(colors::kWine, 0.9f);
  c[ImGuiCol_ButtonHovered] = Float4(colors::kCrimson);
  c[ImGuiCol_ButtonActive] = Float4(colors::kRed);
  c[ImGuiCol_Header] = Float4(colors::kWine, 0.85f);
  c[ImGuiCol_HeaderHovered] = Float4(colors::kCrimson, 0.7f);
  c[ImGuiCol_HeaderActive] = Float4(colors::kCrimson);
  c[ImGuiCol_Separator] = Float4(colors::kBrass, 0.6f);
  c[ImGuiCol_SeparatorHovered] = Float4(colors::kGoldDark);
  c[ImGuiCol_SeparatorActive] = Float4(colors::kGold);
  c[ImGuiCol_ResizeGrip] = Float4(colors::kBrass, 0.35f);
  c[ImGuiCol_ResizeGripHovered] = Float4(colors::kGoldDark, 0.7f);
  c[ImGuiCol_ResizeGripActive] = Float4(colors::kGold);
  c[ImGuiCol_InputTextCursor] = Float4(colors::kGold);
  c[ImGuiCol_Tab] = Float4(colors::kWine, 0.6f);
  c[ImGuiCol_TabHovered] = Float4(colors::kCrimson);
  c[ImGuiCol_TabSelected] = Float4(colors::kCrimson, 0.9f);
  c[ImGuiCol_TabSelectedOverline] = Float4(colors::kGold);
  c[ImGuiCol_TabDimmed] = Float4(colors::kWine, 0.4f);
  c[ImGuiCol_TabDimmedSelected] = Float4(colors::kWine);
  c[ImGuiCol_TabDimmedSelectedOverline] = Float4(colors::kGoldDark);
  c[ImGuiCol_PlotLines] = Float4(colors::kGold);
  c[ImGuiCol_PlotLinesHovered] = Float4(colors::kFlame);
  c[ImGuiCol_PlotHistogram] = Float4(colors::kGoldDark);
  c[ImGuiCol_PlotHistogramHovered] = Float4(colors::kGold);
  c[ImGuiCol_TableHeaderBg] = Float4(colors::kWine, 0.7f);
  c[ImGuiCol_TableBorderStrong] = Float4(colors::kBrass, 0.6f);
  c[ImGuiCol_TableBorderLight] = Float4(colors::kBrass, 0.3f);
  c[ImGuiCol_TableRowBg] = Float4(colors::kNight, 0.0f);
  c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
  c[ImGuiCol_TextLink] = Float4(colors::kFlame);
  c[ImGuiCol_TextSelectedBg] = Float4(colors::kCrimson, 0.6f);
  c[ImGuiCol_TreeLines] = Float4(colors::kBrass, 0.5f);
  c[ImGuiCol_DragDropTarget] = Float4(colors::kGold);
  c[ImGuiCol_DragDropTargetBg] = Float4(colors::kGold, 0.1f);
  c[ImGuiCol_NavCursor] = Float4(colors::kGold);
  c[ImGuiCol_NavWindowingHighlight] = Float4(colors::kGold, 0.7f);
  c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);

  style.ScaleAllSizes(at);
  // Scaling rounds sizes down, which takes a one-point border to nothing under
  // a scale of one, so the borders keep a pixel at the least.
  style.FrameBorderSize = std::max(style.FrameBorderSize, 1.0f);
  style.PopupBorderSize = std::max(style.PopupBorderSize, 1.0f);
  style.SeparatorTextBorderSize = std::max(style.SeparatorTextBorderSize, 1.0f);
  style.FontSizeBase = kTextSize;
  style.FontScaleMain = at;
  ImGui::GetStyle() = style;
  scale = at;
}

}  // namespace nanommo
