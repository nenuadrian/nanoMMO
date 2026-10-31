#include "client/combat_text.h"

#include <imgui.h>
#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <utility>
#include <vector>

#include "client/nameplates.h"

namespace nanommo {

namespace {

constexpr Color kHit = {255, 255, 255, 255};
constexpr Color kHurt = {235, 70, 60, 255};
constexpr Color kHeal = {90, 220, 90, 255};
constexpr Color kLevel = {255, 205, 60, 255};

// The others' are smaller, so that your own fight stands out in a crowd.
constexpr float kTheirs = 0.7f;
constexpr float kLevelScale = 1.5f;

// In metres: where a popup starts, over the figure's nameplate, and how far
// it rises over its life.
constexpr float kStart = 0.4f;
constexpr float kRise = 1.2f;

}  // namespace

void CombatText::Add(const protocol::Event& event, uint32_t you) {
  Popup popup{.unit = event.player(),
              .scale = event.player() == you ? 1.0f : kTheirs};
  switch (event.kind()) {
    case protocol::EVENT_HIT:
      popup.text = std::to_string(event.amount());
      popup.unit = event.monster();
      popup.monster = true;
      popup.color = kHit;
      break;
    case protocol::EVENT_HURT:
      popup.text = "-" + std::to_string(event.amount());
      popup.color = kHurt;
      break;
    case protocol::EVENT_HEAL:
      popup.text = "+" + std::to_string(event.amount());
      popup.color = kHeal;
      break;
    case protocol::EVENT_LEVEL:
      popup.text = "Level " + std::to_string(event.amount());
      popup.color = kLevel;
      popup.scale *= kLevelScale;
      break;
    default:
      // A kind from a newer server, which this client can't show.
      return;
  }
  popups_.push_back(std::move(popup));
}

void CombatText::Update(float seconds) {
  for (Popup& popup : popups_) {
    popup.age += seconds;
  }
  std::erase_if(popups_, [](const Popup& popup) { return popup.age >= kLife; });
}

void DrawCombatText(const CombatText& text, const std::vector<Figure>& figures,
                    const Camera3D& camera) {
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  ImFont* font = ImGui::GetFont();
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  for (const Popup& popup : text.popups()) {
    const auto figure =
        std::ranges::find_if(figures, [&popup](const Figure& figure) {
          return figure.id == popup.unit && figure.monster == popup.monster &&
                 !figure.npc;
        });
    if (figure == figures.end()) {
      continue;
    }
    const float share = popup.age / CombatText::kLife;
    const Vector3 over{
        figure->position.x,
        figure->position.y + Above(*figure) + kStart + kRise * share,
        figure->position.z};
    // Behind the camera, GetWorldToScreen would put it on the screen.
    if (Vector3DotProduct(Vector3Subtract(over, camera.position), forward) <=
        0.0f) {
      continue;
    }
    const Vector2 at = GetWorldToScreen(over, camera);
    // Each fades out over the second half of its life.
    const float alpha = std::clamp(2.0f * (1.0f - share), 0.0f, 1.0f);
    const float size = ImGui::GetFontSize() * 2.4f * popup.scale;
    const ImVec2 extent =
        font->CalcTextSizeA(size, FLT_MAX, 0.0f, popup.text.c_str());
    const ImVec2 corner{at.x - extent.x / 2, at.y - extent.y};
    draw->AddText(font, size, {corner.x + 2, corner.y + 2},
                  IM_COL32(0, 0, 0, static_cast<int>(200 * alpha)),
                  popup.text.c_str());
    draw->AddText(font, size, corner,
                  IM_COL32(popup.color.r, popup.color.g, popup.color.b,
                           static_cast<int>(255 * alpha)),
                  popup.text.c_str());
  }
}

}  // namespace nanommo
