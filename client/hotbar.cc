#include "client/hotbar.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <set>
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
// As most games show a skill without a target.
constexpr ImVec4 kNoTarget{0.95f, 0.3f, 0.25f, 1.0f};

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
  // The swings Space goes on into are Space's, so they get no slot.
  std::set<uint32_t> follow;
  for (const auto& [id, skill] : skills.by_id) {
    follow.insert(skill.next());
  }
  for (const auto& [id, skill] : skills.by_id) {
    if (id != skills.attack && !follow.contains(id)) {
      slots_.push_back(skill);
    }
  }
  std::ranges::stable_sort(slots_, {}, &skills::Skill::level);
  if (std::ssize(slots_) > kMostSlots) {
    slots_.resize(kMostSlots);
  }
  const auto swing = skills.by_id.find(skills.attack);
  if (swing != skills.by_id.end()) {
    slots_.insert(slots_.begin(), swing->second);
    swing_ = true;
  }
}

std::optional<uint32_t> Hotbar::Build(const std::vector<Figure>& figures,
                                      const protocol::World& world,
                                      uint32_t target) const {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  // Our level comes with our name, once a second, so the bar waits for it.
  if (you == figures.end() || you->level == 0 || slots_.empty()) {
    return std::nullopt;
  }
  const bool picked = std::ranges::any_of(figures, [target](const Figure& f) {
    return f.monster && f.id == target && f.to != protocol::MOTION_DEAD;
  });
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
  // Each slot's place in the order of the keys: the swing's on Space, and the
  // number keys count from the next, a row of them, then the row above.
  const std::size_t first = swing_ ? 1 : 0;
  const std::size_t keyed = slots_.size() - first;
  // The row above is drawn first, over the numbers, past the swing's slot.
  std::vector<std::size_t> order;
  for (std::size_t i = kRow; i < keyed; ++i) {
    order.push_back(first + i);
  }
  for (std::size_t slot = 0; slot < std::min(slots_.size(), first + kRow);
       ++slot) {
    order.push_back(slot);
  }
  for (std::size_t place = 0; place < order.size(); ++place) {
    const std::size_t slot = order[place];
    const skills::Skill& skill = slots_[slot];
    const bool reached = you->level >= static_cast<int>(skill.level());
    const bool known =
        std::ranges::find(world.skills(), skill.id()) != world.skills().end();
    const bool above = slot >= first + kRow;
    // The swing starts the row of numbers, under the row above.
    if (place > 0 && !(slot == 0 && keyed > kRow)) {
      ImGui::SameLine();
    }
    if (above && place == 0 && swing_) {
      ImGui::Dummy(kSlot);
      ImGui::SameLine();
    }
    ImGui::PushID(static_cast<int>(skill.id()));
    ImGui::BeginDisabled(!known);
    const skills::Effect& effect = skill.effect();
    const bool no_target =
        known && effect.damage() > 0 && !effect.all() && !picked;
    ImGui::PushFont(nullptr, kIconSize);
    if (no_target) {
      ImGui::PushStyleColor(ImGuiCol_Text, kNoTarget);
    }
    const bool clicked =
        ImGui::Button(Icon(skill.look().icon()).c_str(), kSlot);
    if (no_target) {
      ImGui::PopStyleColor();
    }
    ImGui::PopFont();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      if (effect.weapon()) {
        ImGui::SetTooltip("%s\nA swing of what you hold.",
                          skill.name().c_str());
      } else {
        ImGui::SetTooltip(
            "%s\nFrom a trainer, at level %u. Ready again after %u s.",
            skill.name().c_str(), skill.level(), skill.cooldown_ms() / 1000);
      }
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
    const bool swing = swing_ && slot == 0;
    const int index = static_cast<int>(slot - first);
    const std::string label =
        swing ? "Space" : (above ? "s" : "") + std::to_string(index % kRow + 1);
    draw->AddText({min.x + 4.0f, min.y + 2.0f}, kKey, label.c_str());
    if (clicked || (!swing && key == index)) {
      used = skill.id();
    }
    ImGui::PopID();
  }
  ImGui::End();
  return used;
}

}  // namespace nanommo
