#include "client/player_frame.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// In points, at a scale of one: from the corner of the screen, the portrait's
// radius, and the plate beside it.
constexpr float kMargin = 18.0f;
constexpr float kPortrait = 40.0f;
constexpr float kPlateWidth = 300.0f;
constexpr float kPlateHeight = 66.0f;
// The target's frame, from the top of the screen.
constexpr float kTargetTop = 70.0f;

// Close up on the face, a little from one side, as portraits are.
constexpr Camera3D kPortraitCamera{
    .position = {0.35f, 1.62f, 1.25f},
    .target = {0.0f, 1.5f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .fovy = 30.0f,
    .projection = CAMERA_PERSPECTIVE,
};

constexpr ImGuiWindowFlags kFrame =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

float Share(int value, int most) {
  return most > 0 ? std::clamp(static_cast<float>(value) / most, 0.0f, 1.0f)
                  : 0.0f;
}

// A ring of brass round a circle, with a shadow under it.
void DrawRing(ImDrawList* draw, ImVec2 centre, float radius) {
  draw->AddCircleFilled({centre.x + Ui(2.0f), centre.y + Ui(3.0f)},
                        radius + Ui(4.0f), Faded(IM_COL32_BLACK, 0.45f));
  draw->AddCircle(centre, radius + Ui(2.0f), colors::kBrass, 0, Ui(4.5f));
  draw->AddCircle(centre, radius + Ui(4.3f), colors::kNight, 0, Ui(1.4f));
  draw->AddCircle(centre, radius, Faded(colors::kGoldLight, 0.35f), 0,
                  Ui(1.0f));
}

// The plate beside a portrait, from its middle out to the right, with a window
// over it for the mouse.
void BeginFrame(const char* name, ImVec2 min, ImVec2 max) {
  ImGui::SetNextWindowPos(min);
  ImGui::SetNextWindowSize({max.x - min.x, max.y - min.y});
  ImGui::Begin(name, nullptr, kFrame);
}

}  // namespace

void PlayerFrame::Render(const Renderer& renderer) {
  const std::vector<Figure>& figures = renderer.figures();
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (you == figures.end()) {
    return;
  }
  Figure still = *you;
  still.position = {};
  still.yaw = 0.0f;
  still.from = protocol::MOTION_IDLE;
  still.to = protocol::MOTION_IDLE;
  still.to_time = static_cast<float>(GetTime());
  still.blend = 1.0f;
  portrait_.RenderPreview(renderer, still, kPortraitCamera);
  rendered_ = true;
}

void PlayerFrame::Draw(const std::vector<Figure>& figures,
                       const protocol::World& world) const {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  // Our level comes with our name, once a second, so the frame waits for it.
  if (you == figures.end() || you->level == 0) {
    return;
  }
  const float radius = Ui(kPortrait);
  const ImVec2 centre{Ui(kMargin) + radius + Ui(4.0f),
                      Ui(kMargin) + radius + Ui(4.0f)};
  const ImVec2 plate_min{centre.x, centre.y - Ui(kPlateHeight) / 2.0f};
  const ImVec2 plate_max{centre.x + Ui(kPlateWidth),
                         centre.y + Ui(kPlateHeight) / 2.0f};
  BeginFrame("You", {Ui(kMargin), Ui(kMargin)},
             {plate_max.x, centre.y + radius + Ui(8.0f)});
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawPanel(draw, plate_min, plate_max, 0.92f);

  // The portrait, in its ring.
  draw->AddCircleFilled(centre, radius, colors::kCoal);
  if (rendered_) {
    // OpenGL keeps the rows bottom up, so the UVs turn the picture over.
    draw->AddImageRounded(static_cast<ImTextureID>(portrait_.texture().id),
                          {centre.x - radius, centre.y - radius},
                          {centre.x + radius, centre.y + radius}, {0.0f, 1.0f},
                          {1.0f, 0.0f}, IM_COL32_WHITE, radius);
  }
  DrawRing(draw, centre, radius);

  // The level, on a gem at the portrait's foot.
  const ImVec2 gem{centre.x + radius * 0.72f, centre.y + radius * 0.72f};
  DrawDiamond(draw, gem, Ui(14.0f), colors::kWine, colors::kGold);
  const std::string level = absl::StrCat(you->level);
  ImFont* bold = GetFonts().bold;
  const ImVec2 level_extent =
      bold->CalcTextSizeA(Ui(14.0f), FLT_MAX, 0.0f, level.c_str());
  DrawShadowedText(
      draw, bold, Ui(14.0f),
      {gem.x - level_extent.x / 2.0f, gem.y - level_extent.y / 2.0f},
      colors::kGoldLight, level.c_str());

  // The name, and the bars beside the portrait.
  const float left = centre.x + radius + Ui(12.0f);
  const float right = plate_max.x - Ui(16.0f);
  DrawGradientText(draw, GetFonts().title, Ui(16.0f),
                   {left, plate_min.y + Ui(7.0f)}, colors::kGoldLight,
                   colors::kGold, you->name.c_str());
  const std::string health = absl::StrCat(you->health, " / ", you->max_health);
  const ImVec2 health_min{left, plate_min.y + Ui(30.0f)};
  const ImVec2 health_max{right, health_min.y + Ui(16.0f)};
  DrawBar(draw, health_min, health_max, Share(you->health, you->max_health),
          colors::kHealth, colors::kHealthDark, health.c_str());
  const ImVec2 experience_min{left, health_max.y + Ui(5.0f)};
  const ImVec2 experience_max{right, experience_min.y + Ui(7.0f)};
  DrawBar(draw, experience_min, experience_max,
          Share(static_cast<int>(world.experience()),
                static_cast<int>(world.next_level())),
          colors::kGold, colors::kGoldDark, nullptr);
  ImGui::SetCursorScreenPos(experience_min);
  ImGui::InvisibleButton("##experience", {experience_max.x - experience_min.x,
                                          experience_max.y - experience_min.y});
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Experience: %u / %u", world.experience(),
                      world.next_level());
  }
  ImGui::End();
}

void DrawTargetFrame(const std::vector<Figure>& figures, uint32_t target) {
  const auto picked = std::ranges::find_if(figures, [target](const Figure& f) {
    return f.monster && f.id == target;
  });
  if (target == 0 || picked == figures.end() || picked->name.empty()) {
    return;
  }
  const float radius = Ui(26.0f);
  // Under the compass, and the distance under it.
  const float middle = Ui(kTargetTop) + Ui(kPlateHeight) / 2.0f;
  const float width = radius + Ui(250.0f);
  const ImVec2 centre{(ImGui::GetIO().DisplaySize.x - width) / 2.0f + radius,
                      middle};
  const ImVec2 plate_min{centre.x, middle - Ui(kPlateHeight) / 2.0f + Ui(6.0f)};
  const ImVec2 plate_max{centre.x + Ui(250.0f),
                         middle + Ui(kPlateHeight) / 2.0f - Ui(6.0f)};
  BeginFrame("Target", {centre.x - radius - Ui(6.0f), plate_min.y - Ui(4.0f)},
             {plate_max.x, plate_max.y + Ui(6.0f)});
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawPanel(draw, plate_min, plate_max, 0.92f);
  draw->AddCircleFilled(centre, radius, colors::kWine);
  DrawRing(draw, centre, radius);
  ImFont* text = GetFonts().text;
  const float icon = Ui(22.0f);
  const ImVec2 icon_extent =
      text->CalcTextSizeA(icon, FLT_MAX, 0.0f, ICON_FA_SKULL);
  DrawShadowedText(
      draw, text, icon,
      {centre.x - icon_extent.x / 2.0f, centre.y - icon_extent.y / 2.0f},
      colors::kGoldLight, ICON_FA_SKULL);

  const float inner = centre.x + radius + Ui(10.0f);
  const float right = plate_max.x - Ui(16.0f);
  DrawGradientText(
      draw, GetFonts().title, Ui(15.0f), {inner, plate_min.y + Ui(6.0f)},
      IM_COL32(255, 170, 150, 255), colors::kEmber, picked->name.c_str());
  const std::string level = absl::StrCat("Level ", picked->level);
  ImFont* bold = GetFonts().bold;
  const ImVec2 level_extent =
      bold->CalcTextSizeA(Ui(13.0f), FLT_MAX, 0.0f, level.c_str());
  DrawShadowedText(draw, bold, Ui(13.0f),
                   {right - level_extent.x, plate_min.y + Ui(8.0f)},
                   colors::kMuted, level.c_str());
  const std::string health =
      absl::StrCat(picked->health, " / ", picked->max_health);
  DrawBar(draw, {inner, plate_min.y + Ui(28.0f)},
          {right, plate_min.y + Ui(42.0f)},
          Share(picked->health, picked->max_health), colors::kHealth,
          colors::kHealthDark, health.c_str());
  ImGui::End();
}

}  // namespace nanommo
