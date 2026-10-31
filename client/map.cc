#include "client/map.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "client/atmosphere.h"
#include "client/ground.h"

namespace nanommo {

namespace {

// Deeper than this, in metres, the sea is the deep colour.
constexpr float kShallow = 1.5f;

// How much the slopes' light and shade show.
constexpr float kRelief = 1.6f;

// In points.
constexpr float kMinimapSize = 200.0f;
constexpr float kMargin = 12.0f;

constexpr ImU32 kOutline = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kPlayer = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kMonster = IM_COL32(235, 90, 75, 255);
constexpr ImU32 kNpc = IM_COL32(250, 210, 70, 255);

ImU32 Opaque(Color color) { return IM_COL32(color.r, color.g, color.b, 255); }

ImU32 Sky(int land) {
  const std::array<float, 3>& sky = kLooks[land].atmosphere.sky;
  return Opaque(ColorFromNormalized({sky[0], sky[1], sky[2], 1.0f}));
}

// Where a place goes on the screen, for a view of the land with its middle at
// centre, where it is on the screen at origin, and scale points to a metre.
struct View {
  Place centre;
  ImVec2 origin;
  float scale = 1.0f;

  ImVec2 operator()(float x, float z) const {
    return {origin.x + (x - centre.x) * scale,
            origin.y + (z - centre.z) * scale};
  }
};

void DrawDot(ImDrawList* draw, ImVec2 at, float radius, ImU32 color) {
  draw->AddCircleFilled(at, radius + 1.0f, kOutline);
  draw->AddCircleFilled(at, radius, color);
}

// The land's gates, its NPCs, monsters and players, the way you're walking,
// and you, last, on top, as an arrow the way you face.
void DrawMarkers(ImDrawList* draw, const View& view, int land,
                 const std::vector<Figure>& figures, uint32_t target,
                 std::optional<Vector3> destination) {
  for (const Gate& gate : kGates) {
    if (gate.land == land) {
      const ImVec2 at = view(gate.place.x, gate.place.z);
      draw->AddCircle(at, 6.0f, kOutline, 0, 4.0f);
      draw->AddCircle(at, 6.0f, Sky(gate.to), 0, 2.0f);
    }
  }
  const Figure* you = nullptr;
  for (const Figure& figure : figures) {
    const ImVec2 at = view(figure.position.x, figure.position.z);
    if (figure.you) {
      you = &figure;
    } else if (figure.npc) {
      if (figure.mark != 0) {
        const char mark[2] = {figure.mark, 0};
        const ImVec2 size = ImGui::CalcTextSize(mark);
        draw->AddText({at.x - size.x / 2 + 1, at.y - size.y / 2 + 1}, kOutline,
                      mark);
        draw->AddText({at.x - size.x / 2, at.y - size.y / 2}, kNpc, mark);
      } else {
        DrawDot(draw, at, 2.5f, kNpc);
      }
    } else if (figure.monster) {
      if (figure.to != protocol::MOTION_DEAD) {
        DrawDot(draw, at, 2.0f, kMonster);
      }
      if (figure.id == target) {
        draw->AddCircle(at, 5.0f, kMonster, 0, 1.5f);
      }
    } else {
      DrawDot(draw, at, 2.5f, kPlayer);
    }
  }
  if (destination) {
    const ImVec2 at = view(destination->x, destination->z);
    for (const ImU32 color : {kOutline, kPlayer}) {
      const float width = color == kOutline ? 3.5f : 1.5f;
      draw->AddLine({at.x - 4, at.y - 4}, {at.x + 4, at.y + 4}, color, width);
      draw->AddLine({at.x - 4, at.y + 4}, {at.x + 4, at.y - 4}, color, width);
    }
  }
  if (you != nullptr) {
    // A yaw of zero faces along z, which is down the map.
    const ImVec2 at = view(you->position.x, you->position.z);
    const float s = std::sin(you->yaw);
    const float c = std::cos(you->yaw);
    const auto turned = [&](float across, float along) {
      return ImVec2{at.x + along * s + across * c,
                    at.y + along * c - across * s};
    };
    const ImVec2 tip = turned(0.0f, 8.0f);
    const ImVec2 left = turned(-5.0f, -5.0f);
    const ImVec2 right = turned(5.0f, -5.0f);
    draw->AddTriangleFilled(tip, left, right, kPlayer);
    draw->AddTriangle(tip, left, right, kOutline, 1.5f);
  }
}

}  // namespace

LandMap::~LandMap() {
  if (texture_.id != 0) {
    UnloadTexture(texture_);
  }
}

Image DrawLand(const Terrain& terrain, int land) {
  const Look& look = kLooks[land];
  const float half = terrain.half_size();
  const int size = static_cast<int>(2.0f * half);
  Image image = GenImageColor(size, size, BLACK);
  auto* pixels = static_cast<Color*>(image.data);
  const float inside = half - 1.01f;
  const auto height = [&terrain, inside](float x, float z) {
    return terrain.Height(std::clamp(x, -inside, inside),
                          std::clamp(z, -inside, inside));
  };
  for (int down = 0; down < size; ++down) {
    for (int across = 0; across < size; ++across) {
      const float x = static_cast<float>(across) + 0.5f - half;
      const float z = static_cast<float>(down) + 0.5f - half;
      const float here = height(x, z);
      Color color;
      if (here < Terrain::kSeaLevel) {
        color = here < Terrain::kSeaLevel - kShallow ? look.sea.deep
                                                     : look.sea.shallow;
      } else {
        // The slope across and down, in metres a metre.
        const float dx = (height(x + 1.0f, z) - height(x - 1.0f, z)) / 2.0f;
        const float dz = (height(x, z + 1.0f) - height(x, z - 1.0f)) / 2.0f;
        const float up = 1.0f / std::sqrt(1.0f + dx * dx + dz * dz);
        color = GroundColor(look, here, up);
        // Lit from the top left, as maps are: slopes that face it lighter,
        // and the others darker.
        const float light =
            std::clamp(1.0f + (dx + dz) * kRelief * up, 0.55f, 1.35f);
        color = {static_cast<unsigned char>(std::min(color.r * light, 255.0f)),
                 static_cast<unsigned char>(std::min(color.g * light, 255.0f)),
                 static_cast<unsigned char>(std::min(color.b * light, 255.0f)),
                 255};
      }
      color.a = 255;
      pixels[down * size + across] = color;
    }
  }
  // Each tree as a dark spot, in the trees' colour.
  const Color tree = ColorTint({40, 80, 40, 255}, look.trees);
  for (const Tree& planted : terrain.trees()) {
    const int across = static_cast<int>(planted.x + half);
    const int down = static_cast<int>(planted.z + half);
    for (int j = down - 1; j <= down; ++j) {
      for (int i = across - 1; i <= across; ++i) {
        if (i >= 0 && j >= 0 && i < size && j < size) {
          pixels[j * size + i] = tree;
        }
      }
    }
  }
  return image;
}

void LandMap::Build(const Terrain& terrain, int land) {
  land_ = land;
  half_ = terrain.half_size();
  Image image = DrawLand(terrain, land);
  if (texture_.id != 0) {
    UnloadTexture(texture_);
  }
  texture_ = LoadTextureFromImage(image);
  UnloadImage(image);
  SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
  // Past the edge is more sea, rather than the other side of the island.
  SetTextureWrap(texture_, TEXTURE_WRAP_CLAMP);
}

void LandMap::DrawMinimap(const std::vector<Figure>& figures, uint32_t target,
                          std::optional<Vector3> destination) const {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (texture_.id == 0 || you == figures.end()) {
    return;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x - kMargin, screen.y - kMargin},
                          ImGuiCond_Always, {1.0f, 1.0f});
  ImGui::Begin("Minimap", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  const char* name = kLands[land_].name;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       (kMinimapSize - ImGui::CalcTextSize(name).x) / 2.0f);
  ImGui::TextUnformatted(name);
  const float size = 2.0f * half_;
  const Place centre{you->position.x, you->position.z};
  const ImVec2 uv0{(centre.x - kAround + half_) / size,
                   (centre.z - kAround + half_) / size};
  const ImVec2 uv1{(centre.x + kAround + half_) / size,
                   (centre.z + kAround + half_) / size};
  ImGui::Image(static_cast<ImTextureID>(texture_.id),
               {kMinimapSize, kMinimapSize}, uv0, uv1);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(min, max, true);
  DrawMarkers(draw,
              {.centre = centre,
               .origin = {(min.x + max.x) / 2, (min.y + max.y) / 2},
               .scale = kMinimapSize / (2.0f * kAround)},
              land_, figures, target, destination);
  draw->PopClipRect();
  draw->AddRect(min, max, kOutline, 0.0f, 0, 2.0f);
  ImGui::End();
}

std::optional<Place> LandMap::DrawMap(const std::vector<Figure>& figures,
                                      uint32_t target,
                                      std::optional<Vector3> destination,
                                      bool* open) const {
  if (texture_.id == 0 || !*open) {
    return std::nullopt;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float size = std::min(screen.x, screen.y) * 0.75f;
  ImGui::SetNextWindowPos({screen.x / 2, screen.y / 2}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  const std::string title = std::string("The ") + kLands[land_].name + "###Map";
  ImGui::Begin(title.c_str(), open,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::Image(static_cast<ImTextureID>(texture_.id), {size, size});
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  const View view{.centre = {0.0f, 0.0f},
                  .origin = {(min.x + max.x) / 2, (min.y + max.y) / 2},
                  .scale = size / (2.0f * half_)};
  std::optional<Place> clicked;
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      const ImVec2 mouse = ImGui::GetMousePos();
      clicked = Place{.x = (mouse.x - view.origin.x) / view.scale,
                      .z = (mouse.y - view.origin.y) / view.scale};
    }
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(min, max, true);
  DrawMarkers(draw, view, land_, figures, target, destination);
  draw->PopClipRect();
  ImGui::TextDisabled("Click somewhere to walk there. M closes the map.");
  ImGui::End();
  return clicked;
}

}  // namespace nanommo
