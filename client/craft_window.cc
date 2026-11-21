#include "client/craft_window.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/icon.h"
#include "client/theme.h"

namespace nanommo {

namespace {

// In points, at a scale of one: the bar's size, and how far its top is from
// the bottom of the screen, clear of the hotbar's two rows.
constexpr float kBarWidth = 300.0f;
constexpr float kBarHeight = 18.0f;
constexpr float kBarAbove = 200.0f;

constexpr ImVec4 kTitle{1.0f, 0.82f, 0.3f, 1.0f};
// What there's enough of, in green, and what there isn't, faint.
constexpr ImVec4 kEnough{0.49f, 0.84f, 0.44f, 1.0f};
constexpr ImVec4 kShort{0.56f, 0.48f, 0.44f, 1.0f};

// The item's icon and name, or a question mark for one the items file
// doesn't have.
std::string Named(const ItemTable& items, uint32_t id) {
  const auto item = items.by_id.find(id);
  if (item == items.by_id.end()) {
    return "?";
  }
  return absl::StrCat(Icon(item->second.look().icon()), "  ",
                      item->second.name());
}

}  // namespace

uint32_t HowMany(const protocol::Inventory& bag, uint32_t item) {
  uint32_t count = 0;
  for (const auto& [slot, stack] : bag.bag()) {
    if (stack.item() == item) {
      count += CountOf(stack);
    }
  }
  return count;
}

bool CanMake(const recipes::Recipe& recipe, const protocol::Inventory& bag,
             int level) {
  return level >= static_cast<int>(recipe.level()) &&
         std::ranges::all_of(
             recipe.inputs(), [&bag](const recipes::ItemCount& input) {
               return HowMany(bag, input.item()) >= input.count();
             });
}

float MadeShare(const recipes::Recipe& recipe, uint32_t done) {
  const float whole = 1000.0f * static_cast<float>(recipe.seconds());
  return whole > 0.0f ? std::min(static_cast<float>(done) / whole, 1.0f) : 1.0f;
}

std::optional<protocol::CraftAction> CraftWindow::Build(
    const std::vector<Figure>& figures, const protocol::Inventory& bag,
    const protocol::Crafting& making) {
  if (making.recipe() != 0) {
    DrawProgress(making);
  }
  if (!visible_) {
    return std::nullopt;
  }
  const auto you = std::ranges::find_if(figures, &Figure::you);
  const int level = you != figures.end() ? you->level : 0;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2, display.y * 0.45f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(360.0f), 0.0f});
  BeginPanel("Crafting", &visible_,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  std::optional<protocol::CraftAction> asked;
  for (const auto& [id, recipe] : content_.recipes) {
    // They know each of their level or below.
    if (static_cast<int>(recipe.level()) > level) {
      continue;
    }
    ImGui::PushID(static_cast<int>(id));
    ImGui::PushFont(GetFonts().title, 17.0f);
    ImGui::TextColored(kTitle, "%s", recipe.name().c_str());
    ImGui::PopFont();
    for (const recipes::ItemCount& input : recipe.inputs()) {
      const uint32_t have = HowMany(bag, input.item());
      ImGui::TextColored(
          have >= input.count() ? kEnough : kShort, "%s  %u / %u",
          Named(content_.items, input.item()).c_str(), have, input.count());
    }
    ImGui::Text("Makes %s x%u, in %u s",
                Named(content_.items, recipe.output().item()).c_str(),
                recipe.output().count(), recipe.seconds());
    if (GameButton("Craft", {Ui(90.0f), Ui(28.0f)}, true,
                   making.recipe() == 0 && CanMake(recipe, bag, level))) {
      asked.emplace();
      asked->set_start(id);
    }
    ImGui::PopID();
    ImGui::Spacing();
  }
  // Moving stops it too, and so does this.
  if (making.recipe() != 0 && GameButton("Stop", {Ui(90.0f), Ui(28.0f)})) {
    asked.emplace();
    asked->set_stop(true);
  }
  EndPanel();
  return asked;
}

void CraftWindow::DrawProgress(const protocol::Crafting& making) const {
  const auto recipe = content_.recipes.find(making.recipe());
  if (recipe == content_.recipes.end()) {
    return;
  }
  const auto item = content_.items.by_id.find(recipe->second.output().item());
  const std::string name =
      item != content_.items.by_id.end() ? item->second.name() : "";
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  const float width = Ui(kBarWidth);
  const ImVec2 min{(display.x - width) / 2.0f, display.y - Ui(kBarAbove)};
  const ImVec2 max{min.x + width, min.y + Ui(kBarHeight)};
  // In a panel, as everything else on the screen is.
  const float around = Ui(10.0f);
  DrawPanel(draw, {min.x - around, min.y - around},
            {max.x + around, max.y + around});
  DrawBar(draw, min, max, MadeShare(recipe->second, making.done()),
          colors::kFlame, colors::kCrimson, name.c_str());
}

}  // namespace nanommo
