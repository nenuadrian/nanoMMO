#include "client/character_screen.h"

#include <imgui.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "client/bag_window.h"
#include "extras/IconsFontAwesome6.h"
#include "protocol/places.h"

namespace nanommo {

namespace {

constexpr float kWornSlot = 44.0f;

// Four metres in front of the character, level with its middle.
constexpr Camera3D kPreviewCamera{
    .position = {0.0f, 0.9f, 4.0f},
    .target = {0.0f, 0.9f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .fovy = 35.0f,
    .projection = CAMERA_PERSPECTIVE,
};

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
  idle.yaw = 0.0f;
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
  ImGui::SetNextWindowSize({640.0f, 400.0f}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Character", &visible_)) {
    // OpenGL keeps the rows bottom up, so the UVs turn the picture over.
    ImGui::Image(static_cast<ImTextureID>(preview_.texture().id),
                 {kPreviewPixels, kPreviewPixels}, {0.0f, 1.0f}, {1.0f, 0.0f});
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s", you->name.c_str());
    ImGui::TextDisabled("Level %d", you->level);
    ImGui::Separator();
    ImGui::Text(ICON_FA_HEART " Health: %d / %d", you->health, you->max_health);
    ImGui::Text(ICON_FA_STAR " Experience: %u / %u", world.experience(),
                world.next_level());
    ImGui::Text(ICON_FA_LOCATION_CROSSHAIRS " %s, at %.1f, %.1f", land,
                you->position.x, you->position.z);
    ImGui::Separator();
    ImGui::TextDisabled("Worn");
    for (const items::Slot slot :
         {items::SLOT_MAIN_HAND, items::SLOT_OFF_HAND, items::SLOT_HEAD,
          items::SLOT_BODY, items::SLOT_FEET}) {
      if (slot != items::SLOT_MAIN_HAND) {
        ImGui::SameLine();
      }
      if (std::optional<protocol::ItemAction> done =
              ItemSlot(items_, bag, Worn(slot), kWornSlot)) {
        action = std::move(done);
      }
    }
    ImGui::EndGroup();
  }
  ImGui::End();
  return action;
}

}  // namespace nanommo
