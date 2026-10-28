#include "client/bag_window.h"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/icon.h"
#include "protocol/places.h"

namespace nanommo {

namespace {

constexpr char kPayload[] = "ITEM";
constexpr float kIconSize = 24.0f;
constexpr float kBagSlot = 48.0f;
constexpr int kColumns = 6;
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kCount = IM_COL32(240, 240, 240, 255);
// Font Awesome's circle-question, for an item this client doesn't have.
constexpr uint32_t kUnknownIcon = 0xF059;

// Where a dragged item comes from. ImGui copies a payload byte for byte, so
// it's plain data, not the Place message.
struct Dragged {
  bool worn = false;
  uint32_t slot = 0;
};

protocol::Place PlaceOf(const Dragged& dragged) {
  return dragged.worn ? Worn(static_cast<items::Slot>(dragged.slot))
                      : InBag(dragged.slot);
}

uint32_t Key(const protocol::Place& place) {
  return place.has_bag() ? place.bag() : static_cast<uint32_t>(place.worn());
}

bool SamePlace(const protocol::Place& a, const protocol::Place& b) {
  return a.where_case() == b.where_case() && Key(a) == Key(b);
}

const items::Stack* StackAt(const protocol::Inventory& bag,
                            const protocol::Place& place) {
  const auto& slots = place.has_bag() ? bag.bag() : bag.worn();
  const auto found = slots.find(Key(place));
  return found != slots.end() ? &found->second : nullptr;
}

const items::Item* Find(const ItemTable& items, uint32_t id) {
  const auto found = items.by_id.find(id);
  return found != items.by_id.end() ? &found->second : nullptr;
}

const char* StatName(items::Stat stat) {
  switch (stat) {
    case items::STAT_DAMAGE:
      return "damage";
    case items::STAT_HEALTH:
      return "health";
    case items::STAT_ARMOUR:
      return "armour";
    default:
      return "nothing";
  }
}

std::string Added(const items::Modifier& modifier) {
  return absl::StrCat(modifier.amount() >= 0 ? "+" : "", modifier.amount(), " ",
                      StatName(modifier.stat()));
}

// An empty slot that's worn shows what goes in it, greyed out.
uint32_t SlotIcon(items::Slot slot) {
  switch (slot) {
    case items::SLOT_MAIN_HAND:
      return 0xF6DE;  // hand-fist
    case items::SLOT_OFF_HAND:
      return 0xF132;  // shield
    case items::SLOT_HEAD:
      return 0xF8C0;  // hat-cowboy
    case items::SLOT_BODY:
      return 0xF553;  // shirt
    case items::SLOT_LEGS:
      return 0xF554;  // person-walking
    case items::SLOT_FEET:
      return 0xF696;  // socks
    default:
      return kUnknownIcon;
  }
}

// 0xRRGGBBAA, as the content has colours.
ImVec4 ColorOf(uint32_t rgba) {
  return {static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
          static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
          static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
          static_cast<float>(rgba & 0xFF) / 255.0f};
}

protocol::ItemAction MoveTo(const protocol::Place& from,
                            const protocol::Place& to, uint32_t count = 0) {
  protocol::ItemAction action;
  *action.mutable_move()->mutable_from() = from;
  *action.mutable_move()->mutable_to() = to;
  action.mutable_move()->set_count(count);
  return action;
}

}  // namespace

std::vector<std::string> ItemLines(const ItemTable& items,
                                   const items::Stack& stack) {
  const items::Item* item = Find(items, stack.item());
  if (item == nullptr) {
    return {"An item this client doesn't know"};
  }
  std::vector<std::string> lines = {item->name()};
  if (item->has_equip()) {
    lines.push_back(absl::StrCat("Worn: ", SlotName(item->equip().slot())));
  }
  for (const items::Modifier& modifier : item->equip().modifiers()) {
    lines.push_back(Added(modifier));
  }
  for (const items::Modifier& roll : stack.rolls()) {
    lines.push_back(absl::StrCat(Added(roll), ", rolled"));
  }
  if (item->has_use()) {
    std::string use = absl::StrCat("Use: heals ", item->use().heal());
    if (item->use().cooldown_ms() > 0) {
      absl::StrAppend(&use, ", one every ", item->use().cooldown_ms() / 1000,
                      " s");
    }
    lines.push_back(use);
  }
  if (StackSize(*item) > 1) {
    lines.push_back(absl::StrCat("Stacks up to ", StackSize(*item)));
  }
  return lines;
}

std::optional<uint32_t> FirstEmptySlot(const protocol::Inventory& bag) {
  for (uint32_t slot = 0; slot < kBagSlots; ++slot) {
    if (!bag.bag().contains(slot)) {
      return slot;
    }
  }
  return std::nullopt;
}

std::optional<protocol::ItemAction> QuickAction(const ItemTable& items,
                                                const protocol::Inventory& bag,
                                                const protocol::Place& place) {
  const items::Stack* stack = StackAt(bag, place);
  const items::Item* item =
      stack != nullptr ? Find(items, stack->item()) : nullptr;
  if (item == nullptr) {
    return std::nullopt;
  }
  if (place.has_worn()) {
    if (const std::optional<uint32_t> empty = FirstEmptySlot(bag)) {
      return MoveTo(place, InBag(*empty));
    }
    return std::nullopt;
  }
  if (item->has_use()) {
    protocol::ItemAction action;
    action.set_use(place.bag());
    return action;
  }
  if (item->has_equip()) {
    return MoveTo(place, Worn(item->equip().slot()));
  }
  return std::nullopt;
}

std::optional<protocol::ItemAction> ItemSlot(const ItemTable& items,
                                             const protocol::Inventory& bag,
                                             const protocol::Place& place,
                                             float size) {
  const items::Stack* stack = StackAt(bag, place);
  const items::Item* item =
      stack != nullptr ? Find(items, stack->item()) : nullptr;
  std::optional<protocol::ItemAction> action;
  ImGui::PushID(place.has_bag() ? static_cast<int>(place.bag())
                                : 100 + static_cast<int>(place.worn()));

  ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  uint32_t icon = kUnknownIcon;
  if (item != nullptr) {
    icon = item->look().icon();
    if (item->look().has_tint()) {
      color = ColorOf(item->look().tint());
    }
  } else if (stack == nullptr) {
    icon = place.has_worn() ? SlotIcon(place.worn()) : 0;
    color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::PushFont(nullptr, kIconSize);
  ImGui::Button(absl::StrCat(icon != 0 ? Icon(icon) : "", "##slot").c_str(),
                {size, size});
  ImGui::PopFont();
  ImGui::PopStyleColor();

  if (stack != nullptr && stack->count() > 1) {
    const std::string count = absl::StrCat(stack->count());
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 extent = ImGui::CalcTextSize(count.c_str());
    const ImVec2 corner{max.x - extent.x - 3.0f, max.y - extent.y - 1.0f};
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddText({corner.x + 1, corner.y + 1}, kShadow, count.c_str());
    draw->AddText(corner, kCount, count.c_str());
  }

  if (stack != nullptr && ImGui::BeginDragDropSource()) {
    const Dragged dragged{.worn = place.has_worn(), .slot = Key(place)};
    ImGui::SetDragDropPayload(kPayload, &dragged, sizeof(dragged));
    ImGui::Text("%s", item != nullptr ? item->name().c_str() : "Unknown item");
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayload)) {
      Dragged dragged;
      std::memcpy(&dragged, payload->Data, sizeof(dragged));
      const protocol::Place from = PlaceOf(dragged);
      // With Shift, half the stack goes, and the rest stays.
      const items::Stack* moving = StackAt(bag, from);
      const uint32_t count =
          ImGui::GetIO().KeyShift && moving != nullptr && moving->count() > 1
              ? moving->count() / 2
              : 0;
      if (!SamePlace(from, place)) {
        action = MoveTo(from, place, count);
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (stack != nullptr && ImGui::IsItemHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    action = QuickAction(items, bag, place);
  }
  if (ImGui::IsItemHovered() && ImGui::GetDragDropPayload() == nullptr &&
      !ImGui::IsPopupOpen("menu")) {
    ImGui::BeginTooltip();
    if (stack == nullptr) {
      ImGui::TextDisabled(
          "%s", place.has_worn() ? std::string(SlotName(place.worn())).c_str()
                                 : "Empty");
    } else {
      const std::vector<std::string> lines = ItemLines(items, *stack);
      ImGui::Text("%s", lines.front().c_str());
      for (std::size_t i = 1; i < lines.size(); ++i) {
        ImGui::TextDisabled("%s", lines[i].c_str());
      }
    }
    ImGui::EndTooltip();
  }

  if (stack != nullptr && ImGui::BeginPopupContextItem("menu")) {
    if (place.has_bag() && item != nullptr && item->has_use() &&
        ImGui::MenuItem("Use")) {
      action.emplace().set_use(place.bag());
    }
    if (place.has_bag() && item != nullptr && item->has_equip() &&
        ImGui::MenuItem("Wear")) {
      action = MoveTo(place, Worn(item->equip().slot()));
    }
    const std::optional<uint32_t> empty = FirstEmptySlot(bag);
    // Greyed out when the bag has no room for it.
    if (place.has_worn() &&
        ImGui::MenuItem("Take off", nullptr, false, empty.has_value()) &&
        empty) {
      action = MoveTo(place, InBag(*empty));
    }
    if (ImGui::MenuItem("Throw away")) {
      *action.emplace().mutable_destroy() = place;
    }
    ImGui::EndPopup();
  }
  ImGui::PopID();
  return action;
}

std::optional<protocol::ItemAction> BagWindow::Build(
    const protocol::Inventory& bag) {
  if (!visible_) {
    return std::nullopt;
  }
  constexpr float kMargin = 10.0f;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x - kMargin, display.y * 0.5f},
                          ImGuiCond_FirstUseEver, {1.0f, 0.5f});
  std::optional<protocol::ItemAction> action;
  if (ImGui::Begin(
          "Bag", &visible_,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    for (uint32_t slot = 0; slot < kBagSlots; ++slot) {
      if (slot % kColumns != 0) {
        ImGui::SameLine();
      }
      if (std::optional<protocol::ItemAction> done =
              ItemSlot(items_, bag, InBag(slot), kBagSlot)) {
        action = std::move(done);
      }
    }
  }
  ImGui::End();
  return action;
}

}  // namespace nanommo
