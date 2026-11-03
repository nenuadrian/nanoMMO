#include "client/theme.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <raylib.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <utility>

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
  // Our panels draw their own borders.
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 0.0f;
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

void PathPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, float cut) {
  draw->PathLineTo({min.x + cut, min.y});
  draw->PathLineTo({max.x - cut, min.y});
  draw->PathLineTo({max.x, min.y + cut});
  draw->PathLineTo({max.x, max.y - cut});
  draw->PathLineTo({max.x - cut, max.y});
  draw->PathLineTo({min.x + cut, max.y});
  draw->PathLineTo({min.x, max.y - cut});
  draw->PathLineTo({min.x, min.y + cut});
}

void DrawDiamond(ImDrawList* draw, ImVec2 centre, float radius, ImU32 fill,
                 ImU32 outline) {
  const ImVec2 top{centre.x, centre.y - radius};
  const ImVec2 right{centre.x + radius, centre.y};
  const ImVec2 bottom{centre.x, centre.y + radius};
  const ImVec2 left{centre.x - radius, centre.y};
  draw->AddQuadFilled(top, right, bottom, left, fill);
  draw->AddQuad(top, right, bottom, left, outline, std::max(1.0f, Ui(1.0f)));
}

void DrawPanel(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha) {
  const float cut = Ui(10.0f);
  // A shadow, a little below and to the right, so that it lifts off the world.
  const ImVec2 drop = Ui(ImVec2{3.0f, 4.0f});
  PathPanel(draw, {min.x + drop.x, min.y + drop.y},
            {max.x + drop.x, max.y + drop.y}, cut);
  draw->PathFillConvex(Faded(IM_COL32_BLACK, 0.45f * alpha));

  // Dark, a little lighter at the top, as if lit from above.
  const int first = draw->VtxBuffer.Size;
  PathPanel(draw, min, max, cut);
  draw->PathFillConvex(Faded(colors::kNight, alpha));
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(
      draw, first, draw->VtxBuffer.Size, min, {min.x, max.y}, colors::kCoal,
      colors::kNight);

  // A red glow under the title.
  const float glow = std::min(Ui(46.0f), (max.y - min.y) * 0.5f);
  draw->AddRectFilledMultiColor(
      {min.x + cut * 0.6f, min.y + Ui(2.0f)},
      {max.x - cut * 0.6f, min.y + glow}, Faded(colors::kWine, 0.55f * alpha),
      Faded(colors::kWine, 0.55f * alpha), Faded(colors::kWine, 0.0f),
      Faded(colors::kWine, 0.0f));

  // Brass round it, and a thin light inside the brass.
  PathPanel(draw, min, max, cut);
  draw->PathStroke(Faded(colors::kBrass, alpha), ImDrawFlags_Closed, Ui(1.6f));
  const float inset = Ui(3.5f);
  PathPanel(draw, {min.x + inset, min.y + inset},
            {max.x - inset, max.y - inset}, cut * 0.75f);
  draw->PathStroke(Faded(colors::kGoldLight, 0.12f * alpha), ImDrawFlags_Closed,
                   std::max(1.0f, Ui(0.8f)));

  // A gem at each of the top corners, where the corner's cut.
  const float gem = Ui(4.0f);
  for (const ImVec2 at : {ImVec2{min.x + cut * 0.5f, min.y + cut * 0.5f},
                          ImVec2{max.x - cut * 0.5f, min.y + cut * 0.5f}}) {
    DrawDiamond(draw, at, gem, Faded(colors::kEmber, alpha),
                Faded(colors::kGold, alpha));
  }
}

void DrawDivider(ImDrawList* draw, ImVec2 left, float width) {
  const float middle = left.x + width / 2.0f;
  const float thickness = std::max(1.0f, Ui(1.0f));
  draw->AddRectFilledMultiColor({left.x, left.y}, {middle, left.y + thickness},
                                Faded(colors::kGoldDark, 0.0f),
                                colors::kGoldDark, colors::kGoldDark,
                                Faded(colors::kGoldDark, 0.0f));
  draw->AddRectFilledMultiColor(
      {middle, left.y}, {left.x + width, left.y + thickness}, colors::kGoldDark,
      Faded(colors::kGoldDark, 0.0f), Faded(colors::kGoldDark, 0.0f),
      colors::kGoldDark);
  DrawDiamond(draw, {middle, left.y + thickness / 2.0f}, Ui(3.5f),
              colors::kNight, colors::kGold);
}

void DrawShadowedText(ImDrawList* draw, ImFont* font, float size, ImVec2 at,
                      ImU32 color, const char* text) {
  const float offset = std::max(1.0f, size / 16.0f);
  draw->AddText(
      font, size, {at.x + offset, at.y + offset},
      Faded(colors::kShadow,
            static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f),
      text);
  draw->AddText(font, size, at, color, text);
}

void DrawGradientText(ImDrawList* draw, ImFont* font, float size, ImVec2 at,
                      ImU32 top, ImU32 bottom, const char* text) {
  const float offset = std::max(1.0f, size / 14.0f);
  const float alpha =
      static_cast<float>((top >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
  draw->AddText(font, size, {at.x + offset, at.y + offset},
                Faded(IM_COL32_BLACK, 0.85f * alpha), text);
  const int first = draw->VtxBuffer.Size;
  draw->AddText(font, size, at, top, text);
  // The glyphs' boxes are taller than their capitals, so the gradient runs over
  // the middle of the line, where the letters are.
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(
      draw, first, draw->VtxBuffer.Size, {at.x, at.y + size * 0.2f},
      {at.x, at.y + size * 0.85f}, top, bottom);
}

void DrawTitle(ImDrawList* draw, ImVec2 top_centre, float size,
               const char* text) {
  ImFont* font = GetFonts().title;
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  DrawGradientText(draw, font, size,
                   {top_centre.x - extent.x / 2.0f, top_centre.y},
                   colors::kGoldLight, colors::kGoldDark, text);
}

std::string Shown(const char* label) {
  const std::string all = label;
  return all.substr(0, all.find("##"));
}

namespace {

// The height of a panel's title, above what's in it.
float TitleHeight() { return Ui(34.0f); }

}  // namespace

bool BeginPanel(const char* name, bool* open, ImGuiWindowFlags flags,
                float alpha) {
  // Its background, title and close button are ours, drawn here.
  const bool shown = ImGui::Begin(
      name, nullptr,
      flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 min = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 max{min.x + size.x, min.y + size.y};
  // Under everything in the window, as nothing else is drawn in it yet.
  draw->PushClipRectFullScreen();
  DrawPanel(draw, min, max, alpha);
  const std::string title = Shown(name);
  const float title_size = Ui(19.0f);
  if (!title.empty()) {
    DrawTitle(draw, {min.x + size.x / 2.0f, min.y + Ui(9.0f)}, title_size,
              title.c_str());
    DrawDivider(draw, {min.x + Ui(18.0f), min.y + TitleHeight()},
                size.x - Ui(36.0f));
  }
  draw->PopClipRect();

  if (open != nullptr) {
    // A small square with an X in the top right corner, as far in as the
    // window's padding: any further out, and a window that fits what's in it
    // would grow to fit the button, a little more each frame.
    const float button = Ui(20.0f);
    const float inset = ImGui::GetStyle().WindowPadding.x;
    const ImVec2 corner{max.x - button - inset, min.y + Ui(8.0f)};
    const ImVec2 saved = ImGui::GetCursorPos();
    ImGui::SetCursorScreenPos(corner);
    if (ImGui::InvisibleButton("##close", {button, button})) {
      *open = false;
    }
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 middle{corner.x + button / 2.0f, corner.y + button / 2.0f};
    const float arm = button * 0.22f;
    const ImU32 ink = hovered ? colors::kEmber : colors::kMuted;
    if (hovered) {
      draw->AddCircleFilled(middle, button * 0.5f, Faded(colors::kWine, 0.9f));
    }
    draw->AddLine({middle.x - arm, middle.y - arm},
                  {middle.x + arm, middle.y + arm}, ink, Ui(2.0f));
    draw->AddLine({middle.x - arm, middle.y + arm},
                  {middle.x + arm, middle.y - arm}, ink, Ui(2.0f));
    ImGui::SetCursorPos(saved);
  }
  if (!title.empty()) {
    // What's in the panel starts under its title.
    ImGui::SetCursorPosY(
        std::max(ImGui::GetCursorPosY(), TitleHeight() + Ui(10.0f)));
  }
  return shown;
}

void EndPanel() { ImGui::End(); }

bool GameButton(const char* label, ImVec2 size, bool primary, bool enabled) {
  const std::string text = Shown(label);
  ImFont* font = GetFonts().title;
  const float text_size = primary ? Ui(17.0f) : Ui(15.0f);
  const ImVec2 extent =
      font->CalcTextSizeA(text_size, FLT_MAX, 0.0f, text.c_str());
  if (size.x < 0.0f) {
    size.x = ImGui::GetContentRegionAvail().x;
  } else if (size.x == 0.0f) {
    size.x = extent.x + Ui(36.0f);
  }
  if (size.y <= 0.0f) {
    size.y = primary ? Ui(44.0f) : Ui(34.0f);
  }
  ImGui::BeginDisabled(!enabled);
  const bool clicked = ImGui::InvisibleButton(label, size) && enabled;
  ImGui::EndDisabled();
  const bool hovered = enabled && ImGui::IsItemHovered();
  const bool held = enabled && ImGui::IsItemActive();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float cut = std::min(Ui(7.0f), size.y * 0.3f);

  ImU32 top = primary ? colors::kRed : IM_COL32(64, 14, 21, 255);
  ImU32 bottom = primary ? colors::kWine : IM_COL32(28, 8, 11, 255);
  if (primary && hovered) {
    top = colors::kEmber;
    bottom = colors::kCrimson;
  } else if (hovered) {
    top = IM_COL32(110, 20, 30, 255);
    bottom = IM_COL32(48, 10, 15, 255);
  }
  if (held) {
    std::swap(top, bottom);
  }
  const float alpha = enabled ? 1.0f : 0.45f;
  if (primary && hovered) {
    // A glow round it, like a fire's.
    for (int ring = 3; ring >= 1; --ring) {
      const float spread = Ui(2.5f) * static_cast<float>(ring);
      PathPanel(draw, {min.x - spread, min.y - spread},
                {max.x + spread, max.y + spread}, cut + spread);
      draw->PathFillConvex(Faded(colors::kEmber, 0.08f));
    }
  }
  const int first = draw->VtxBuffer.Size;
  PathPanel(draw, min, max, cut);
  draw->PathFillConvex(Faded(top, alpha));
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(
      draw, first, draw->VtxBuffer.Size, min, {min.x, max.y}, top, bottom);
  // A sheen along its top half.
  draw->AddRectFilledMultiColor(
      {min.x + cut, min.y + Ui(1.5f)}, {max.x - cut, (min.y + max.y) / 2.0f},
      Faded(IM_COL32_WHITE, 0.10f * alpha),
      Faded(IM_COL32_WHITE, 0.10f * alpha), Faded(IM_COL32_WHITE, 0.0f),
      Faded(IM_COL32_WHITE, 0.0f));
  PathPanel(draw, min, max, cut);
  draw->PathStroke(
      Faded(hovered ? colors::kGold
                    : (primary ? colors::kGoldDark : colors::kBrass),
            alpha),
      ImDrawFlags_Closed, Ui(primary ? 1.6f : 1.2f));
  const ImVec2 at{(min.x + max.x - extent.x) / 2.0f,
                  (min.y + max.y - extent.y) / 2.0f + (held ? Ui(1.0f) : 0.0f)};
  DrawGradientText(draw, font, text_size, at, Faded(colors::kGoldLight, alpha),
                   Faded(hovered ? colors::kGold : colors::kGoldDark, alpha),
                   text.c_str());
  return clicked;
}

void DrawBar(ImDrawList* draw, ImVec2 min, ImVec2 max, float share, ImU32 color,
             ImU32 dark, const char* text) {
  share = std::clamp(share, 0.0f, 1.0f);
  draw->AddRectFilled(min, max, Faded(colors::kNight, 0.9f));
  if (share > 0.0f) {
    const ImVec2 full{min.x + (max.x - min.x) * share, max.y};
    draw->AddRectFilledMultiColor(min, full, color, color, dark, dark);
    // Light along the top of what's filled.
    draw->AddRectFilledMultiColor(
        min, {full.x, min.y + (max.y - min.y) * 0.45f},
        Faded(IM_COL32_WHITE, 0.25f), Faded(IM_COL32_WHITE, 0.25f),
        Faded(IM_COL32_WHITE, 0.0f), Faded(IM_COL32_WHITE, 0.0f));
    // A bright edge where it ends.
    draw->AddLine({full.x, min.y}, {full.x, max.y},
                  Faded(colors::kGoldLight, 0.6f), std::max(1.0f, Ui(1.0f)));
  }
  draw->AddRect(min, max, colors::kBrass, 0.0f, 0, std::max(1.0f, Ui(1.0f)));
  if (text != nullptr && text[0] != '\0') {
    ImFont* font = GetFonts().bold;
    const float size = std::min(Ui(15.0f), (max.y - min.y) * 0.95f);
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    DrawShadowedText(
        draw, font, size,
        {(min.x + max.x - extent.x) / 2.0f, (min.y + max.y - extent.y) / 2.0f},
        colors::kPaper, text);
  }
}

void DrawSlot(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 edge) {
  draw->AddRectFilled({min.x + Ui(1.0f), min.y + Ui(2.0f)},
                      {max.x + Ui(1.0f), max.y + Ui(2.0f)},
                      Faded(IM_COL32_BLACK, 0.5f));
  // Darker at the top, as a well's shadow falls.
  draw->AddRectFilledMultiColor(
      min, max, IM_COL32(6, 3, 4, 240), IM_COL32(6, 3, 4, 240),
      IM_COL32(34, 16, 18, 240), IM_COL32(34, 16, 18, 240));
  draw->AddRect(min, max, colors::kBrass, 0.0f, 0, std::max(1.0f, Ui(1.4f)));
  const float inset = Ui(2.5f);
  draw->AddRect({min.x + inset, min.y + inset}, {max.x - inset, max.y - inset},
                Faded(colors::kNight, 0.9f), 0.0f, 0, std::max(1.0f, Ui(1.0f)));
  if (((edge >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
    draw->AddRect({min.x - Ui(1.0f), min.y - Ui(1.0f)},
                  {max.x + Ui(1.0f), max.y + Ui(1.0f)}, edge, 0.0f, 0,
                  Ui(2.2f));
    draw->AddRectFilledMultiColor({min.x + inset, min.y + inset},
                                  {max.x - inset, max.y - inset},
                                  Faded(edge, 0.0f), Faded(edge, 0.0f),
                                  Faded(edge, 0.22f), Faded(edge, 0.22f));
  }
}

void DrawSlotIcon(ImDrawList* draw, ImVec2 min, ImVec2 max, const char* icon,
                  ImU32 color, float alpha) {
  const ImVec2 middle{(min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f};
  const float size = (max.y - min.y) * 0.48f;
  // A glow of the icon's colour behind it, fading out from the middle.
  const float glow = (max.y - min.y) * 0.42f;
  for (int ring = 4; ring >= 1; --ring) {
    draw->AddCircleFilled(middle, glow * static_cast<float>(ring) / 4.0f,
                          Faded(color, 0.05f * alpha));
  }
  ImFont* font = GetFonts().text;
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, icon);
  DrawShadowedText(draw, font, size,
                   {middle.x - extent.x / 2.0f, middle.y - extent.y / 2.0f},
                   Faded(color, alpha), icon);
}

}  // namespace nanommo
