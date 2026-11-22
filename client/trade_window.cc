#include "client/trade_window.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "client/bag_window.h"
#include "client/theme.h"
#include "client/translations.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// In points, at a scale of one.
constexpr float kSlot = 46.0f;
constexpr float kBetween = 28.0f;
// Each side shows eight slots, or as many as it offers, four to a row.
constexpr std::size_t kSlots = 8;
constexpr std::size_t kColumns = 4;

// The side's places in the bag, in order, as a map's come in none.
std::vector<uint32_t> PlacesOf(const protocol::TradeSide& side) {
  std::vector<uint32_t> places;
  for (const auto& [place, stack] : side.bag()) {
    places.push_back(place);
  }
  std::ranges::sort(places);
  return places;
}

std::size_t SlotsFor(std::size_t offered) {
  return std::max(kSlots, (offered + kColumns - 1) / kColumns * kColumns);
}

}  // namespace

protocol::TradeOffer OfferOf(const protocol::TradeSide& side) {
  protocol::TradeOffer offer;
  const std::vector<uint32_t> places = PlacesOf(side);
  offer.mutable_bag()->Add(places.begin(), places.end());
  offer.set_gold(side.gold());
  return offer;
}

std::optional<protocol::TradeAction> TradeWindow::Build(
    const protocol::Trade& trade) {
  if (trade.partner().empty()) {
    typing_ = false;
    return std::nullopt;
  }
  const protocol::TradeSide& yours = trade.yours();
  const protocol::TradeSide& theirs = trade.theirs();
  const char* partner = trade.partner().c_str();
  std::optional<protocol::TradeAction> action;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.45f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  bool open = true;
  // The same window, whoever it's with.
  const std::string title =
      absl::StrCat(absl::Substitute(Tr("Trade with $0"), partner), "###Trade");
  if (BeginPanel(title.c_str(), &open,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
    const float slot = Ui(kSlot);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Ui(ImVec2{5.0f, 5.0f}));

    // Yours: what's dragged onto it from the bag goes in, and a click on one
    // takes it back. Each change is a new offer, in place of this one.
    protocol::TradeOffer offer = OfferOf(yours);
    bool changed = false;
    const std::vector<uint32_t> offered = PlacesOf(yours);
    ImGui::BeginGroup();
    ImGui::TextUnformatted(Tr("You offer"));
    for (std::size_t i = 0; i < SlotsFor(offered.size()); ++i) {
      if (i % kColumns != 0) {
        ImGui::SameLine();
      }
      ImGui::PushID(static_cast<int>(i));
      const items::Stack* stack =
          i < offered.size() ? &yours.bag().at(offered[i]) : nullptr;
      if (StackSlot(items_, stack, slot, yours.ready() ? colors::kGood : 0) &&
          stack != nullptr) {
        offer.clear_bag();
        for (const uint32_t place : offered) {
          if (place != offered[i]) {
            offer.add_bag(place);
          }
        }
        changed = true;
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const std::optional<protocol::Place> from = AcceptItem();
            from && from->has_bag() && !yours.bag().contains(from->bag())) {
          offer.add_bag(from->bag());
          changed = true;
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::PopID();
    }
    // What's typed stays as it's typed, and goes once it's done with.
    if (!typing_) {
      gold_ = yours.gold();
    }
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors::kGold),
                       ICON_FA_COINS);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(Ui(110.0f));
    ImGui::InputScalar(TrLabel("gold##yours"), ImGuiDataType_U32, &gold_);
    typing_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      offer.set_gold(gold_);
      changed = true;
    }
    ImGui::EndGroup();
    if (changed) {
      *action.emplace().mutable_offer() = offer;
    }

    // Theirs, to look at.
    ImGui::SameLine(0.0f, Ui(kBetween));
    ImGui::BeginGroup();
    ImGui::TextUnformatted(absl::Substitute(Tr("$0 offers"), partner).c_str());
    const std::vector<uint32_t> places = PlacesOf(theirs);
    for (std::size_t i = 0; i < SlotsFor(places.size()); ++i) {
      if (i % kColumns != 0) {
        ImGui::SameLine();
      }
      ImGui::PushID(static_cast<int>(100 + i));
      StackSlot(items_,
                i < places.size() ? &theirs.bag().at(places[i]) : nullptr, slot,
                theirs.ready() ? colors::kGood : 0);
      ImGui::PopID();
    }
    ImGui::TextColored(
        ImGui::ColorConvertU32ToFloat4(colors::kGold), ICON_FA_COINS " %s",
        absl::Substitute(TrN("$0 gold", "$0 gold", theirs.gold()),
                         theirs.gold())
            .c_str());
    ImGui::EndGroup();
    ImGui::PopStyleVar();

    ImGui::Spacing();
    if (GameButton(yours.ready() ? "Not ready" : "Ready", {Ui(150.0f), 0.0f},
                   !yours.ready())) {
      action.emplace().set_ready(!yours.ready());
      action->set_revision(trade.revision());
    }
    ImGui::SameLine();
    if (theirs.ready()) {
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors::kGood), "%s",
                         absl::Substitute(Tr("$0 is ready."), partner).c_str());
    } else {
      ImGui::TextDisabled(
          "%s", absl::Substitute(Tr("$0 isn't ready yet."), partner).c_str());
    }
  }
  EndPanel();
  if (!open) {
    action.emplace().set_cancel(true);
  }
  return action;
}

}  // namespace nanommo
