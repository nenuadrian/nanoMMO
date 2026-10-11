#include "client/nameplates.h"

#include <imgui.h>
#include <raylib.h>

#include <cfloat>

#include "client/camera.h"

namespace nanommo {

void DrawNameplates(const protocol::World& world) {
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize() * 1.4f;
  for (const protocol::Player& player : world.players()) {
    const Vector2 at =
        GetWorldToScreen({player.x(), 1.9f, player.z()}, GameCamera());
    const char* name = player.name().c_str();
    const ImVec2 size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, name);
    const ImVec2 corner{at.x - size.x / 2, at.y - size.y};
    draw->AddText(font, font_size, {corner.x + 1, corner.y + 1},
                  IM_COL32(0, 0, 0, 200), name);
    draw->AddText(font, font_size, corner, IM_COL32_WHITE, name);
  }
}

}  // namespace nanommo
