#include "client/nameplates.h"

#include <imgui.h>
#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace nanommo {

namespace {

constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kLevel = IM_COL32(190, 190, 190, 255);
constexpr ImU32 kMonsterName = IM_COL32(255, 140, 120, 255);
constexpr ImU32 kNpcName = IM_COL32(150, 230, 140, 255);
constexpr ImU32 kPlayerHealth = IM_COL32(90, 200, 90, 255);
constexpr ImU32 kMonsterHealth = IM_COL32(220, 70, 60, 255);

constexpr ImVec2 kBar{64.0f, 6.0f};

// Further than this, in metres, figures fade into the fog, and their
// nameplates would float over the hills in front of them.
constexpr float kFarthest = 50.0f;

void DrawShadowedText(ImDrawList* draw, float size, ImVec2 at, ImU32 color,
                      const std::string& text) {
  ImFont* font = ImGui::GetFont();
  draw->AddText(font, size, {at.x + 1, at.y + 1}, kShadow, text.c_str());
  draw->AddText(font, size, at, color, text.c_str());
}

// In metres above a figure's feet.
float Above(const Figure& figure) {
  if (figure.npc) {
    return 2.0f * figure.radius + 0.4f;
  }
  return figure.monster ? 1.9f : 2.35f;
}

}  // namespace

void DrawNameplates(const std::vector<Figure>& figures,
                    const Camera3D& camera) {
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize() * 1.4f;
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  for (const Figure& figure : figures) {
    // Names come once a second, so one we've just met waits for its nameplate.
    if (figure.name.empty()) {
      continue;
    }
    const Vector3& position = figure.position;
    const Vector3 away = Vector3Subtract(position, camera.position);
    // Behind the camera, GetWorldToScreen would put it on the screen.
    if (Vector3Length(away) > kFarthest ||
        Vector3DotProduct(away, forward) <= 0.0f) {
      continue;
    }
    const Vector2 at = GetWorldToScreen(
        {position.x, position.y + Above(figure), position.z}, camera);
    // NPCs don't fight, so they have a name and no level or health.
    if (figure.npc) {
      const ImVec2 size =
          font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, figure.name.c_str());
      DrawShadowedText(draw, font_size, {at.x - size.x / 2, at.y - size.y},
                       kNpcName, figure.name);
      continue;
    }

    const ImVec2 bar_min{at.x - kBar.x / 2, at.y - kBar.y};
    const ImVec2 bar_max{at.x + kBar.x / 2, at.y};
    const float share =
        figure.max_health > 0
            ? std::clamp(static_cast<float>(figure.health) / figure.max_health,
                         0.0f, 1.0f)
            : 0.0f;
    draw->AddRectFilled(bar_min, bar_max, kShadow);
    draw->AddRectFilled(bar_min, {bar_min.x + kBar.x * share, bar_max.y},
                        figure.monster ? kMonsterHealth : kPlayerHealth);

    const std::string level = std::to_string(figure.level) + " ";
    const float level_width =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, level.c_str()).x;
    const ImVec2 name_size =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, figure.name.c_str());
    const float left = at.x - (level_width + name_size.x) / 2;
    const float top = bar_min.y - 2.0f - name_size.y;
    DrawShadowedText(draw, font_size, {left, top}, kLevel, level);
    DrawShadowedText(draw, font_size, {left + level_width, top},
                     figure.monster ? kMonsterName : IM_COL32_WHITE,
                     figure.name);
  }
}

}  // namespace nanommo
