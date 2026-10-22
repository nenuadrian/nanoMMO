#include "client/hotbar.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "client/icon.h"
#include "client/input.h"

namespace nanommo {

namespace {

constexpr ImVec2 kSlot{56.0f, 56.0f};
constexpr float kIconSize = 26.0f;
constexpr float kMargin = 12.0f;
constexpr ImU32 kShade = IM_COL32(0, 0, 0, 170);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 255);
constexpr float kSecondsSize = 22.0f;
constexpr ImU32 kKey = IM_COL32(230, 230, 230, 255);

// With a shadow, so that it reads over the icon.
void DrawCentered(ImDrawList* draw, ImVec2 at, float size,
                  const std::string& text) {
  ImFont* font = ImGui::GetFont();
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
  const ImVec2 corner{at.x - extent.x / 2, at.y - extent.y / 2};
  draw->AddText(font, size, {corner.x + 1, corner.y + 1}, kShadow,
                text.c_str());
  draw->AddText(font, size, corner, kKey, text.c_str());
}

// Shades the share of the slot that's still cooling down, as a slice that
// shrinks clockwise from the top, with the seconds left in the middle.
void DrawCooldown(ImDrawList* draw, ImVec2 min, ImVec2 max, float share,
                  uint32_t milliseconds) {
  constexpr float kTop = -std::numbers::pi_v<float> / 2;
  constexpr float kTurn = 2 * std::numbers::pi_v<float>;
  const ImVec2 middle{(min.x + max.x) / 2, (min.y + max.y) / 2};
  draw->PushClipRect(min, max, true);
  draw->PathLineTo(middle);
  draw->PathArcTo(middle, max.x - min.x, kTop + kTurn * (1.0f - share),
                  kTop + kTurn, 32);
  draw->PathFillConcave(kShade);
  draw->PopClipRect();
  DrawCentered(draw, middle, kSecondsSize,
               std::to_string(static_cast<int>(
                   std::ceil(static_cast<float>(milliseconds) / 1000.0f))));
}

}  // namespace

Hotbar::Hotbar(const SkillTable& skills) {
  for (const auto& [id, skill] : skills.by_id) {
    slots_.push_back(skill);
  }
  std::ranges::stable_sort(slots_, {}, &skills::Skill::level);
  if (slots_.size() > kMostSlots) {
    slots_.resize(kMostSlots);
  }
}

std::optional<uint32_t> Hotbar::Build(const std::vector<Figure>& figures,
                                      const protocol::World& world) const {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  // Our level comes with our name, once a second, so the bar waits for it.
  if (you == figures.end() || you->level == 0 || slots_.empty()) {
    return std::nullopt;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x / 2, screen.y - kMargin}, ImGuiCond_Always,
                          {0.5f, 1.0f});
  ImGui::Begin("Hotbar", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  const std::optional<int> key = HotbarKey();
  std::optional<uint32_t> used;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
    const skills::Skill& skill = slots_[slot];
    const bool reached = you->level >= static_cast<int>(skill.level());
    const bool known =
        std::ranges::find(world.skills(), skill.id()) != world.skills().end();
    if (slot > 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(static_cast<int>(skill.id()));
    ImGui::BeginDisabled(!known);
    ImGui::PushFont(nullptr, kIconSize);
    const bool clicked =
        ImGui::Button(Icon(skill.look().icon()).c_str(), kSlot);
    ImGui::PopFont();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip(
          "%s\nFrom a trainer, at level %u. Ready again after %u s.",
          skill.name().c_str(), skill.level(), skill.cooldown_ms() / 1000);
    }
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const auto left = world.cooldowns().find(skill.id());
    if (!known) {
      DrawCentered(draw, {(min.x + max.x) / 2, max.y - 10.0f},
                   ImGui::GetFontSize(),
                   reached ? "Learn" : "Lv " + std::to_string(skill.level()));
    } else if (left != world.cooldowns().end() && skill.cooldown_ms() > 0) {
      DrawCooldown(draw, min, max,
                   std::min(static_cast<float>(left->second) /
                                static_cast<float>(skill.cooldown_ms()),
                            1.0f),
                   left->second);
    }
    draw->AddText({min.x + 4.0f, min.y + 2.0f}, kKey,
                  std::to_string(slot + 1).c_str());
    if (clicked || key == static_cast<int>(slot)) {
      used = skill.id();
    }
    ImGui::PopID();
  }
  ImGui::End();
  return used;
}

}  // namespace nanommo
