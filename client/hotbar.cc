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
#include "client/theme.h"

namespace nanommo {

namespace {

// In points, at a scale of one.
constexpr float kSlot = 54.0f;
constexpr float kGap = 6.0f;
constexpr float kMargin = 14.0f;
constexpr float kPadding = 12.0f;

// As most games show a skill without a target.
constexpr ImU32 kNoTarget = IM_COL32(242, 77, 64, 255);

// A skill's colour, which its icon's in: its first fx's, as it looks in the
// world, or else the paper of the words.
ImU32 SkillColor(const skills::Skill& skill) {
  for (const skills::Fx& fx : skill.look().fx()) {
    if (fx.color() != 0) {
      const uint32_t rgba = fx.color();
      return IM_COL32((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                      (rgba >> 8) & 0xFF, 255);
    }
  }
  return colors::kPaper;
}

void DrawCentered(ImDrawList* draw, ImFont* font, ImVec2 at, float size,
                  ImU32 color, const std::string& text) {
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
  DrawShadowedText(draw, font, size, {at.x - extent.x / 2, at.y - extent.y / 2},
                   color, text.c_str());
}

// Shades the share of the slot that's still cooling down, as a slice that
// shrinks clockwise from the top, with the seconds left in the middle.
void DrawCooldown(ImDrawList* draw, ImVec2 min, ImVec2 max, float share,
                  uint32_t milliseconds) {
  constexpr float kTop = -std::numbers::pi_v<float> / 2;
  constexpr float kTurn = 2 * std::numbers::pi_v<float>;
  const ImVec2 middle{(min.x + max.x) / 2, (min.y + max.y) / 2};
  // Inside the slot's brass.
  const float inset = Ui(2.5f);
  draw->PushClipRect({min.x + inset, min.y + inset},
                     {max.x - inset, max.y - inset}, true);
  draw->PathLineTo(middle);
  draw->PathArcTo(middle, max.x - min.x, kTop + kTurn * (1.0f - share),
                  kTop + kTurn, 32);
  draw->PathFillConcave(IM_COL32(0, 0, 0, 175));
  // The edge of the slice, bright, as it sweeps round.
  const float edge = kTop + kTurn * (1.0f - share);
  draw->AddLine(middle,
                {middle.x + std::cos(edge) * (max.x - min.x),
                 middle.y + std::sin(edge) * (max.x - min.x)},
                Faded(colors::kGold, 0.6f), Ui(1.2f));
  draw->PopClipRect();
  DrawCentered(draw, GetFonts().bold, middle, Ui(22.0f), colors::kGoldLight,
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
  const float slot_size = Ui(kSlot);
  const float gap = Ui(kGap);
  const float padding = Ui(kPadding);
  // The swing and the number keys along the bottom, and the row on Shift
  // above them, past the swing's slot.
  const std::size_t first = swing_ ? 1 : 0;
  const std::size_t keyed = slots_.size() - first;
  const bool two_rows = keyed > kRow;
  const float columns =
      static_cast<float>(std::min(slots_.size(), first + kRow));
  const float rows = two_rows ? 2.0f : 1.0f;
  const ImVec2 size{
      columns * slot_size + (columns - 1.0f) * gap + 2.0f * padding,
      rows * slot_size + (rows - 1.0f) * gap + 2.0f * padding};
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x / 2, screen.y - Ui(kMargin)},
                          ImGuiCond_Always, {0.5f, 1.0f});
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
  ImGui::Begin("Hotbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 corner = ImGui::GetWindowPos();
  DrawPanel(draw, corner, {corner.x + size.x, corner.y + size.y}, 0.9f);

  const std::optional<int> key = HotbarKey();
  std::optional<uint32_t> used;
  for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
    const skills::Skill& skill = slots_[slot];
    const bool reached = you->level >= static_cast<int>(skill.level());
    const bool known =
        std::ranges::find(world.skills(), skill.id()) != world.skills().end();
    const skills::Effect& effect = skill.effect();
    const bool no_target =
        known && effect.damage() > 0 && !effect.all() && !picked;
    // The swing stays on Space, at the front.
    const bool swing = swing_ && slot == 0;
    const bool above = slot >= first + kRow;
    const std::size_t column = above ? slot - kRow : slot;
    const float row = above || !two_rows ? 0.0f : 1.0f;
    const ImVec2 min{
        corner.x + padding + static_cast<float>(column) * (slot_size + gap),
        corner.y + padding + row * (slot_size + gap)};
    const ImVec2 max{min.x + slot_size, min.y + slot_size};
    ImGui::PushID(static_cast<int>(skill.id()));
    ImGui::SetCursorScreenPos(min);
    const bool clicked =
        ImGui::InvisibleButton("##slot", {slot_size, slot_size}) && known;
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImU32 edge = 0;
    if (hovered) {
      edge = held ? colors::kEmber : colors::kGold;
    }

    DrawSlot(draw, min, max, edge);
    const ImU32 color = no_target ? kNoTarget : SkillColor(skill);
    DrawSlotIcon(draw, min, max, Icon(skill.look().icon()).c_str(), color,
                 known ? 1.0f : 0.3f);
    if (hovered) {
      if (effect.weapon()) {
        ImGui::SetTooltip("%s\nA swing of what you hold.",
                          skill.name().c_str());
      } else {
        ImGui::SetTooltip(
            "%s\nFrom a trainer, at level %u. Ready again after %u s.",
            skill.name().c_str(), skill.level(), skill.cooldown_ms() / 1000);
      }
    }
    const auto left = world.cooldowns().find(skill.id());
    if (!known) {
      DrawCentered(draw, GetFonts().bold,
                   {(min.x + max.x) / 2, max.y - Ui(11.0f)}, Ui(13.0f),
                   reached ? colors::kGold : colors::kMuted,
                   reached ? "Learn" : "Lv " + std::to_string(skill.level()));
    } else if (left != world.cooldowns().end() && skill.cooldown_ms() > 0) {
      DrawCooldown(draw, min, max,
                   std::min(static_cast<float>(left->second) /
                                static_cast<float>(skill.cooldown_ms()),
                            1.0f),
                   left->second);
    }
    // The swing's on Space, and the number keys count from the next, with
    // Shift for the row above.
    const int index = static_cast<int>(slot - first);
    const std::string label =
        swing ? "Space"
              : (above ? "s" : "") + std::to_string(index % kRow + 1);
    DrawShadowedText(draw, GetFonts().bold, Ui(13.0f),
                     {min.x + Ui(5.0f), min.y + Ui(2.0f)}, colors::kPaper,
                     label.c_str());
    if (clicked || (!swing && key == index)) {
      used = skill.id();
    }
    ImGui::PopID();
  }
  ImGui::End();
  return used;
}

}  // namespace nanommo
