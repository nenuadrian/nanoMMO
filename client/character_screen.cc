#include "client/character_screen.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/bag_window.h"
#include "client/theme.h"
#include "protocol/places.h"

namespace nanommo {

namespace {

// In points, at a scale of one.
constexpr float kSlot = 52.0f;
constexpr float kGap = 10.0f;
constexpr ImVec2 kStage{250.0f, 328.0f};

// Four metres in front of the character, level with its middle, which shows
// all of it in the preview's shape.
constexpr Camera3D kPreviewCamera{
    .position = {0.0f, 0.95f, 4.2f},
    .target = {0.0f, 0.9f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .fovy = 35.0f,
    .projection = CAMERA_PERSPECTIVE,
};

// What's worn on the body down the left, from the head to the feet, and
// what's held on the right.
constexpr std::array<items::Slot, 4> kLeft = {
    items::SLOT_HEAD, items::SLOT_BODY, items::SLOT_LEGS, items::SLOT_FEET};
constexpr std::array<items::Slot, 2> kRight = {items::SLOT_MAIN_HAND,
                                               items::SLOT_OFF_HAND};

// A glow behind the character, from the floor up, and a ring of light on the
// floor where it stands.
void DrawStage(ImDrawList* draw, ImVec2 min, ImVec2 max) {
  draw->AddRectFilledMultiColor(
      min, max, Faded(colors::kNight, 0.9f), Faded(colors::kNight, 0.9f),
      Faded(colors::kWine, 0.9f), Faded(colors::kWine, 0.9f));
  const ImVec2 floor{(min.x + max.x) / 2.0f, max.y - (max.y - min.y) * 0.06f};
  const float width = (max.x - min.x) * 0.42f;
  for (int ring = 6; ring >= 1; --ring) {
    const float share = static_cast<float>(ring) / 6.0f;
    draw->AddEllipseFilled(floor, {width * share * 1.4f, width * share * 0.5f},
                           Faded(colors::kEmber, 0.05f));
  }
  draw->AddEllipse(floor, {width, width * 0.3f}, Faded(colors::kGoldDark, 0.8f),
                   0.0f, 0, Ui(1.5f));
  draw->AddRect(min, max, colors::kBrass, 0.0f, 0, Ui(1.2f));
}

}  // namespace

void CharacterScreen::Render(const Renderer& renderer) {
  if (!visible_) {
    return;
  }
  const std::vector<Figure>& figures = renderer.figures();
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (you == figures.end()) {
    return;
  }
  Figure idle = *you;
  idle.position = {};
  idle.yaw = 0.3f;
  idle.from = protocol::MOTION_IDLE;
  idle.to = protocol::MOTION_IDLE;
  idle.to_time = static_cast<float>(GetTime());
  idle.blend = 1.0f;
  preview_.RenderPreview(renderer, idle, kPreviewCamera);
}

std::optional<protocol::ItemAction> CharacterScreen::Build(
    const std::vector<Figure>& figures, const protocol::World& world,
    const char* land, const protocol::Inventory& bag) {
  if (!visible_) {
    return std::nullopt;
  }
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (you == figures.end() || you->level == 0) {
    return std::nullopt;
  }
  std::optional<protocol::ItemAction> action;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x * 0.3f, display.y * 0.5f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  if (BeginPanel("Character", &visible_,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
    const float slot = Ui(kSlot);
    const float gap = Ui(kGap);
    const ImVec2 stage = Ui(kStage);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // The stage in the middle, with the character on it.
    const ImVec2 stage_min{origin.x + slot + gap, origin.y};
    const ImVec2 stage_max{stage_min.x + stage.x, stage_min.y + stage.y};
    DrawStage(draw, stage_min, stage_max);
    // OpenGL keeps the rows bottom up, so the UVs turn the picture over.
    draw->AddImage(static_cast<ImTextureID>(preview_.texture().id), stage_min,
                   stage_max, {0.0f, 1.0f}, {1.0f, 0.0f});

    const auto slots = [&](auto column, float x) {
      for (std::size_t i = 0; i < column.size(); ++i) {
        ImGui::SetCursorScreenPos(
            {x, origin.y + static_cast<float>(i) * (slot + gap)});
        if (std::optional<protocol::ItemAction> done =
                ItemSlot(items_, bag, Worn(column[i]), slot)) {
          action = std::move(done);
        }
      }
    };
    slots(kLeft, origin.x);
    slots(kRight, stage_max.x + gap);

    // Under it, who you are, and how you are.
    ImGui::SetCursorScreenPos({origin.x, stage_max.y + gap});
    const float width = stage_max.x + gap + slot - origin.x;
    ImFont* title = GetFonts().title;
    const float name_size = Ui(20.0f);
    const ImVec2 name_extent =
        title->CalcTextSizeA(name_size, FLT_MAX, 0.0f, you->name.c_str());
    const ImVec2 name_at{origin.x + (width - name_extent.x) / 2.0f,
                         stage_max.y + gap};
    DrawGradientText(draw, title, name_size, name_at, colors::kGoldLight,
                     colors::kGold, you->name.c_str());
    const std::string line =
        absl::StrCat("Level ", you->level, ", in the ", land);
    const float line_size = Ui(15.0f);
    ImFont* text = GetFonts().text;
    const ImVec2 line_extent =
        text->CalcTextSizeA(line_size, FLT_MAX, 0.0f, line.c_str());
    DrawShadowedText(draw, text, line_size,
                     {origin.x + (width - line_extent.x) / 2.0f,
                      name_at.y + name_extent.y + Ui(2.0f)},
                     colors::kMuted, line.c_str());
    const float bars = name_at.y + name_extent.y + line_extent.y + Ui(12.0f);
    const std::string health =
        absl::StrCat("Health ", you->health, " / ", you->max_health);
    DrawBar(draw, {origin.x, bars}, {origin.x + width, bars + Ui(18.0f)},
            you->max_health > 0
                ? static_cast<float>(you->health) / you->max_health
                : 0.0f,
            colors::kHealth, colors::kHealthDark, health.c_str());
    const std::string experience = absl::StrCat(
        "Experience ", world.experience(), " / ", world.next_level());
    DrawBar(draw, {origin.x, bars + Ui(24.0f)},
            {origin.x + width, bars + Ui(42.0f)},
            world.next_level() > 0 ? static_cast<float>(world.experience()) /
                                         static_cast<float>(world.next_level())
                                   : 0.0f,
            colors::kGoldDark, IM_COL32(90, 58, 10, 255), experience.c_str());
    ImGui::SetCursorScreenPos({origin.x, bars + Ui(42.0f)});
    ImGui::Dummy({width, Ui(4.0f)});
  }
  EndPanel();
  return action;
}

}  // namespace nanommo
