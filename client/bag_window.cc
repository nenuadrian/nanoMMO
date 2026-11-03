#include "client/bag_window.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/icon.h"
#include "client/theme.h"
#include "protocol/places.h"
#include "server/inventory.h"

namespace nanommo {

namespace {

constexpr char kPayload[] = "ITEM";
// In points, at a scale of one.
constexpr float kBagSlot = 52.0f;
constexpr int kColumns = 6;
// Font Awesome's circle-question, for an item this client doesn't have.
constexpr uint32_t kUnknownIcon = 0xF059;

// Where a dragged item comes from, and where in its slot the mouse took hold
// of it. ImGui copies a payload byte for byte, so it's plain data, not the
// Place message.
struct Dragged {
  bool worn = false;
  uint32_t slot = 0;
  ImVec2 grab;
};

// The item the player's holding, while they drag one.
std::optional<Dragged> Held() {
  const ImGuiPayload* payload = ImGui::GetDragDropPayload();
  if (payload == nullptr || !payload->IsDataType(kPayload)) {
    return std::nullopt;
  }
  Dragged dragged;
  std::memcpy(&dragged, payload->Data, sizeof(dragged));
  return dragged;
}

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

// The item's icon's colour, from its tint, as 0xRRGGBBAA, or the paper's.
ImU32 TintOf(const items::Item* item) {
  if (item == nullptr || !item->look().has_tint()) {
    return colors::kPaper;
  }
  const uint32_t rgba = item->look().tint();
  return IM_COL32((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF,
                  rgba & 0xFF);
}

protocol::ItemAction MoveTo(const protocol::Place& from,
                            const protocol::Place& to, uint32_t count = 0) {
  protocol::ItemAction action;
  *action.mutable_move()->mutable_from() = from;
  *action.mutable_move()->mutable_to() = to;
  action.mutable_move()->set_count(count);
  return action;
}

// Whether the server would move what's at from to to: the bag's own rules,
// which it keeps, tried on a copy.
bool Fits(const ItemTable& items, const protocol::Inventory& bag,
          const protocol::Place& from, const protocol::Place& to) {
  Inventory copy(bag);
  return copy.Move(items, from, to).ok();
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

  ImGui::InvisibleButton("##slot", {size, size});
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();

  // Taken hold of, it follows the mouse, from where it was held.
  if (stack != nullptr &&
      ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
    const ImVec2 down = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
    const Dragged dragged{.worn = place.has_worn(),
                          .slot = Key(place),
                          .grab = {down.x - min.x, down.y - min.y}};
    ImGui::SetDragDropPayload(kPayload, &dragged, sizeof(dragged),
                              ImGuiCond_Once);
    ImGui::EndDragDropSource();
  }
  const std::optional<Dragged> held = Held();
  const bool source = held && SamePlace(PlaceOf(*held), place);

  // Lit green where what's held would go, by the server's own rules, and red
  // where it wouldn't.
  ImU32 edge = 0;
  if (held && !source && ImGui::BeginDragDropTarget()) {
    const protocol::Place from = PlaceOf(*held);
    const bool fits = Fits(items, bag, from, place);
    edge = fits ? colors::kGood : colors::kHealth;
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
            kPayload, ImGuiDragDropFlags_AcceptBeforeDelivery |
                          ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload != nullptr && payload->IsDelivery() && fits) {
      // With Shift, half the stack goes, and the rest stays.
      const items::Stack* moving = StackAt(bag, from);
      const uint32_t count =
          ImGui::GetIO().KeyShift && moving != nullptr && moving->count() > 1
              ? moving->count() / 2
              : 0;
      action = MoveTo(from, place, count);
    }
    ImGui::EndDragDropTarget();
  } else if (hovered && !held) {
    edge = colors::kGold;
  }

  DrawSlot(ImGui::GetWindowDrawList(), min, max, edge);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  // What's held is drawn over the slot under it, so the verdict goes round the
  // slot as well, where it still shows.
  if (held && !source && edge != 0) {
    const float out = Ui(3.0f);
    draw->AddRect({min.x - out, min.y - out}, {max.x + out, max.y + out}, edge,
                  Ui(2.0f), 0, Ui(2.0f));
  }
  if (stack != nullptr) {
    const ImU32 tint = TintOf(item);
    const std::string icon =
        Icon(item != nullptr ? item->look().icon() : kUnknownIcon);
    // Its place stays, faint, while it's held.
    DrawSlotIcon(draw, min, max, icon.c_str(), tint, source ? 0.25f : 1.0f);
    if (stack->count() > 1 && !source) {
      const std::string count = absl::StrCat(stack->count());
      ImFont* bold = GetFonts().bold;
      const float count_size = Ui(14.0f);
      const ImVec2 extent =
          bold->CalcTextSizeA(count_size, FLT_MAX, 0.0f, count.c_str());
      DrawShadowedText(
          draw, bold, count_size,
          {max.x - extent.x - Ui(4.0f), max.y - extent.y - Ui(2.0f)},
          colors::kPaper, count.c_str());
    }
    if (source) {
      DrawDragged(icon.c_str(), tint, size, held->grab);
    }
  } else if (place.has_worn()) {
    // An empty slot that's worn shows what goes in it, faint.
    DrawSlotIcon(draw, min, max, Icon(SlotIcon(place.worn())).c_str(),
                 colors::kMuted, 0.3f);
  }

  if (stack != nullptr && hovered &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    action = QuickAction(items, bag, place);
  }
  if (hovered && !held && !ImGui::IsPopupOpen("menu")) {
    ImGui::BeginTooltip();
    if (stack == nullptr) {
      ImGui::TextDisabled(
          "%s", place.has_worn() ? std::string(SlotName(place.worn())).c_str()
                                 : "Empty");
    } else {
      const std::vector<std::string> lines = ItemLines(items, *stack);
      ImGui::PushFont(GetFonts().title, 15.0f);
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors::kGoldLight),
                         "%s", lines.front().c_str());
      ImGui::PopFont();
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
  std::optional<protocol::ItemAction> action;
  // Let go of over nothing, from the bag or from what's worn, it's thrown
  // away, once the player says so.
  if (const std::optional<Dragged> held = Held();
      held && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
      !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow |
                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    throwing_ = PlaceOf(*held);
  }
  if (throwing_) {
    if (std::optional<protocol::ItemAction> done = BuildThrowAway(bag)) {
      action = std::move(done);
    }
  }
  if (!visible_) {
    return action;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x - Ui(18.0f), display.y * 0.55f},
                          ImGuiCond_FirstUseEver, {1.0f, 0.5f});
  if (BeginPanel("Bag", &visible_,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
    const float slot = Ui(kBagSlot);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Ui(ImVec2{5.0f, 5.0f}));
    for (uint32_t index = 0; index < kBagSlots; ++index) {
      if (index % kColumns != 0) {
        ImGui::SameLine();
      }
      if (std::optional<protocol::ItemAction> done =
              ItemSlot(items_, bag, InBag(index), slot)) {
        action = std::move(done);
      }
    }
    ImGui::PopStyleVar();
    const std::size_t used = bag.bag().size();
    ImGui::TextDisabled(
        "%zu of %u slots used. Drag an item out to throw it "
        "away.",
        used, kBagSlots);
  }
  EndPanel();
  return action;
}

std::optional<protocol::ItemAction> BagWindow::BuildThrowAway(
    const protocol::Inventory& bag) {
  const items::Stack* stack = StackAt(bag, *throwing_);
  const items::Item* item =
      stack != nullptr ? Find(items_, stack->item()) : nullptr;
  if (stack == nullptr) {
    throwing_.reset();
    return std::nullopt;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.42f},
                          ImGuiCond_Always, {0.5f, 0.5f});
  bool open = true;
  std::optional<protocol::ItemAction> action;
  BeginPanel("Throw away?", &open,
             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings);
  const std::string name = item != nullptr ? item->name() : "this item";
  if (stack->count() > 1) {
    ImGui::Text("Throw away all %u of the %s, for good?", stack->count(),
                name.c_str());
  } else {
    ImGui::Text("Throw away the %s, for good?", name.c_str());
  }
  ImGui::Spacing();
  if (GameButton("Throw away", {Ui(150.0f), 0.0f}, true)) {
    *action.emplace().mutable_destroy() = *throwing_;
    open = false;
  }
  ImGui::SameLine();
  if (GameButton("Keep it", {Ui(150.0f), 0.0f}) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    open = false;
  }
  EndPanel();
  if (!open) {
    throwing_.reset();
  }
  return action;
}

}  // namespace nanommo
