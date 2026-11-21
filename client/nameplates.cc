#include "client/nameplates.h"

#include <imgui.h>
#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "client/picking.h"

namespace nanommo {

namespace {

constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kLevel = IM_COL32(190, 190, 190, 255);
constexpr ImU32 kYourName = IM_COL32(255, 210, 80, 255);
constexpr ImU32 kMonsterName = IM_COL32(255, 140, 120, 255);
constexpr ImU32 kNpcName = IM_COL32(150, 230, 140, 255);
constexpr ImU32 kEnemyName = IM_COL32(255, 80, 70, 255);
constexpr ImU32 kGuild = IM_COL32(205, 190, 150, 255);
constexpr float kGuildScale = 0.85f;
// A quest's mark, over an NPC's name, in gold, and twice its size.
constexpr ImU32 kMark = IM_COL32(255, 205, 60, 255);
constexpr float kMarkScale = 2.0f;
constexpr ImU32 kPlayerHealth = IM_COL32(90, 200, 90, 255);
constexpr ImU32 kMonsterHealth = IM_COL32(220, 70, 60, 255);

constexpr ImVec2 kBar{64.0f, 6.0f};
// A monster's skill on its way: a thinner bar, a little under its health, and
// the skill's name, smaller than the monster's.
constexpr float kCastBar = 4.0f;
constexpr float kCastGap = 3.0f;
constexpr float kCastScale = 0.85f;

// Further than this, in metres, figures fade into the fog, and their
// nameplates would float over the hills in front of them.
constexpr float kFarthest = 50.0f;

void DrawShadowedText(ImDrawList* draw, float size, ImVec2 at, ImU32 color,
                      const std::string& text) {
  ImFont* font = ImGui::GetFont();
  draw->AddText(font, size, {at.x + 1, at.y + 1}, kShadow, text.c_str());
  draw->AddText(font, size, at, color, text.c_str());
}

// Yours stands out from the other players', so that you can find yourself in
// a crowd, and an enemy's is red, as they can hurt you.
ImU32 NameColor(const Figure& figure, std::string_view enemy) {
  if (figure.monster) {
    return kMonsterName;
  }
  if (Enemy(figure, enemy)) {
    return kEnemyName;
  }
  return figure.you ? kYourName : IM_COL32_WHITE;
}

}  // namespace

// Over a tall monster's head, such as a boss's.
float Above(const Figure& figure) {
  return figure.monster ? std::max(1.9f, figure.height + 0.3f) : 2.35f;
}

bool HasNameplate(const Figure& figure) {
  return !figure.name.empty() &&
         !(figure.monster && figure.to == protocol::MOTION_DEAD);
}

bool ShowsName(const Figure& figure, const Noticed& noticed) {
  if (noticed.pointed && *noticed.pointed == Who(figure)) {
    return true;
  }
  if (Picked(figure, noticed.target, noticed.target_player)) {
    return true;
  }
  const NameplateSettings& always = noticed.settings;
  if (figure.you) {
    return always.yours;
  }
  if (figure.npc) {
    return always.npcs;
  }
  return figure.monster ? always.monsters : always.players;
}

void DrawNameplates(const std::vector<Figure>& figures, const Camera3D& camera,
                    const std::map<uint32_t, Telegraph>& telegraphs,
                    std::string_view enemy, const Noticed& noticed) {
  const NameplateSettings& with = noticed.settings;
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  ImFont* font = ImGui::GetFont();
  const float font_size = ImGui::GetFontSize() * 1.4f;
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  for (const Figure& figure : figures) {
    if (!HasNameplate(figure)) {
      continue;
    }
    const bool named = ShowsName(figure, noticed);
    // An NPC's quest mark shows without its name.
    if (!named && !(figure.npc && figure.mark != 0)) {
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
      if (named) {
        DrawShadowedText(draw, font_size, {at.x - size.x / 2, at.y - size.y},
                         kNpcName, figure.name);
      }
      if (figure.mark != 0) {
        const std::string mark(1, figure.mark);
        const float mark_size = font_size * kMarkScale;
        const ImVec2 over =
            font->CalcTextSizeA(mark_size, FLT_MAX, 0.0f, mark.c_str());
        DrawShadowedText(draw, mark_size,
                         {at.x - over.x / 2, at.y - size.y - over.y}, kMark,
                         mark);
      }
      continue;
    }

    const ImVec2 bar_min{at.x - kBar.x / 2, at.y - kBar.y};
    const ImVec2 bar_max{at.x + kBar.x / 2, at.y};
    if (with.health) {
      const float share = figure.max_health > 0
                              ? std::clamp(static_cast<float>(figure.health) /
                                               figure.max_health,
                                           0.0f, 1.0f)
                              : 0.0f;
      draw->AddRectFilled(bar_min, bar_max, kShadow);
      draw->AddRectFilled(bar_min, {bar_min.x + kBar.x * share, bar_max.y},
                          figure.monster ? kMonsterHealth : kPlayerHealth);
    }
    // A monster's skill on its way goes under its health, filling as its
    // shape does.
    if (const auto cast = telegraphs.find(figure.id);
        figure.monster && cast != telegraphs.end()) {
      const Telegraph& telegraph = cast->second;
      const Color color = GetColor(telegraph.skill.color());
      const ImVec2 cast_min{bar_min.x, bar_max.y + kCastGap};
      const ImVec2 cast_max{bar_max.x, cast_min.y + kCastBar};
      draw->AddRectFilled(cast_min, cast_max, kShadow);
      draw->AddRectFilled(
          cast_min, {cast_min.x + kBar.x * telegraph.Filled(), cast_max.y},
          IM_COL32(color.r, color.g, color.b, 255));
      const float cast_size = font_size * kCastScale;
      const ImVec2 extent = font->CalcTextSizeA(cast_size, FLT_MAX, 0.0f,
                                                telegraph.skill.name().c_str());
      DrawShadowedText(draw, cast_size, {at.x - extent.x / 2, cast_max.y + 1},
                       IM_COL32_WHITE, telegraph.skill.name());
    }

    const std::string level =
        with.levels ? std::to_string(figure.level) + " " : "";
    const float level_width =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, level.c_str()).x;
    const ImVec2 name_size =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, figure.name.c_str());
    // Over the health, or where it'd be.
    float top = with.health ? bar_min.y - 2.0f : at.y;
    // A player's guild goes under their name, smaller, in the angle brackets
    // games put a guild in.
    if (with.guilds && !figure.guild.empty()) {
      const std::string guild = "<" + figure.guild + ">";
      const float guild_size = font_size * kGuildScale;
      const ImVec2 extent =
          font->CalcTextSizeA(guild_size, FLT_MAX, 0.0f, guild.c_str());
      top -= extent.y;
      DrawShadowedText(draw, guild_size, {at.x - extent.x / 2, top}, kGuild,
                       guild);
    }
    top -= name_size.y;
    const float left = at.x - (level_width + name_size.x) / 2;
    DrawShadowedText(draw, font_size, {left, top}, kLevel, level);
    DrawShadowedText(draw, font_size, {left + level_width, top},
                     NameColor(figure, enemy), figure.name);
  }
}

}  // namespace nanommo
