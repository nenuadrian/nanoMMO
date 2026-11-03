#include "client/map.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/atmosphere.h"
#include "client/ground.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Deeper than this, in metres, the sea is the deep colour.
constexpr float kShallow = 1.5f;

// How much the slopes' light and shade show.
constexpr float kRelief = 1.6f;

// In points, at a scale of one: the minimap's radius, inside its ring, the
// ring's width, and its margin from the screen's corner.
constexpr float kRadius = 100.0f;
constexpr float kRing = 7.0f;
constexpr float kMargin = 18.0f;

constexpr ImU32 kOutline = IM_COL32(0, 0, 0, 210);
constexpr ImU32 kPlayer = IM_COL32(150, 200, 255, 255);
constexpr ImU32 kMonster = IM_COL32(235, 80, 64, 255);
constexpr ImU32 kNpc = IM_COL32(255, 211, 77, 255);

ImU32 Opaque(Color color) { return IM_COL32(color.r, color.g, color.b, 255); }

ImU32 Sky(int land) {
  const std::array<float, 3>& sky = kLooks[land].atmosphere.sky;
  return Opaque(ColorFromNormalized({sky[0], sky[1], sky[2], 1.0f}));
}

// Where a place goes on the screen, for a view of the land with its middle at
// centre, where it is on the screen at origin, at scale points to a metre,
// turned so that the bearing turn is up, which is north when it's zero.
struct View {
  Place centre;
  ImVec2 origin;
  float scale = 1.0f;
  float turn = 0.0f;

  // A way on the ground, as a way on the screen, without its scale.
  ImVec2 Way(float x, float z) const {
    const float c = std::cos(turn);
    const float s = std::sin(turn);
    return {x * c + z * s, -x * s + z * c};
  }

  ImVec2 operator()(float x, float z) const {
    const ImVec2 way = Way((x - centre.x) * scale, (z - centre.z) * scale);
    return {origin.x + way.x, origin.y + way.y};
  }
};

void DrawDot(ImDrawList* draw, ImVec2 at, float radius, ImU32 color) {
  draw->AddCircleFilled(at, radius + Ui(1.2f), kOutline);
  draw->AddCircleFilled(at, radius, color);
}

void DrawGlyph(ImDrawList* draw, ImVec2 at, float size, ImU32 color,
               const char* glyph) {
  ImFont* font = GetFonts().title;
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph);
  const ImVec2 corner{at.x - extent.x / 2.0f, at.y - extent.y / 2.0f};
  for (const ImVec2 offset : {ImVec2{-1, 0}, ImVec2{1, 0}, ImVec2{0, -1},
                              ImVec2{0, 1}, ImVec2{1, 1}}) {
    draw->AddText(font, size, {corner.x + offset.x, corner.y + offset.y},
                  kOutline, glyph);
  }
  draw->AddText(font, size, corner, color, glyph);
}

// Where a mark is on a round map of the radius round the origin: where it is,
// or, if it's past the rim and it's one to keep, on the rim, the way it is,
// or none.
std::optional<ImVec2> OnDisc(ImVec2 at, const View& view, float radius,
                             bool keep) {
  if (radius <= 0.0f) {
    return at;
  }
  const float dx = at.x - view.origin.x;
  const float dy = at.y - view.origin.y;
  const float distance = std::hypot(dx, dy);
  if (distance <= radius) {
    return at;
  }
  if (!keep) {
    return std::nullopt;
  }
  return ImVec2{view.origin.x + dx / distance * radius,
                view.origin.y + dy / distance * radius};
}

// The land's gates, its NPCs, monsters and players, the way you're walking,
// and you, last, on top, as an arrow the way you face. On a round map, of the
// radius, what's past its rim is left out, but for the monster you picked and
// the quests, which wait on the rim.
void DrawMarkers(ImDrawList* draw, const View& view, int land,
                 const std::vector<Figure>& figures, uint32_t target,
                 std::optional<Vector3> destination, float radius = 0.0f) {
  const float rim = radius > 0.0f ? radius - Ui(6.0f) : 0.0f;
  for (const Gate& gate : kGates) {
    if (gate.land == land) {
      if (const std::optional<ImVec2> at =
              OnDisc(view(gate.place.x, gate.place.z), view, rim, false)) {
        draw->AddCircle(*at, Ui(6.0f), kOutline, 0, Ui(4.0f));
        draw->AddCircle(*at, Ui(6.0f), Sky(gate.to), 0, Ui(2.0f));
      }
    }
  }
  const Figure* you = nullptr;
  for (const Figure& figure : figures) {
    const ImVec2 where = view(figure.position.x, figure.position.z);
    if (figure.you) {
      you = &figure;
    } else if (figure.npc) {
      const bool quest = figure.mark != 0;
      const std::optional<ImVec2> at = OnDisc(where, view, rim, quest);
      if (!at) {
        continue;
      }
      if (quest) {
        const char mark[2] = {figure.mark, 0};
        DrawGlyph(draw, *at, Ui(17.0f), kNpc, mark);
      } else {
        DrawDot(draw, *at, Ui(2.8f), kNpc);
      }
    } else if (figure.monster) {
      const bool picked = figure.id == target;
      const std::optional<ImVec2> at = OnDisc(where, view, rim, picked);
      if (!at) {
        continue;
      }
      if (figure.to != protocol::MOTION_DEAD) {
        DrawDot(draw, *at, Ui(2.4f), kMonster);
      }
      if (picked) {
        draw->AddCircle(*at, Ui(5.5f), kOutline, 0, Ui(3.0f));
        draw->AddCircle(*at, Ui(5.5f), colors::kEmber, 0, Ui(1.6f));
      }
    } else if (const std::optional<ImVec2> at =
                   OnDisc(where, view, rim, false)) {
      DrawDot(draw, *at, Ui(2.8f), kPlayer);
    }
  }
  if (destination) {
    if (const std::optional<ImVec2> at =
            OnDisc(view(destination->x, destination->z), view, rim, false)) {
      const float arm = Ui(4.0f);
      for (const ImU32 color : {kOutline, colors::kGoldLight}) {
        const float width = color == kOutline ? Ui(3.5f) : Ui(1.5f);
        draw->AddLine({at->x - arm, at->y - arm}, {at->x + arm, at->y + arm},
                      color, width);
        draw->AddLine({at->x - arm, at->y + arm}, {at->x + arm, at->y - arm},
                      color, width);
      }
    }
  }
  if (you != nullptr) {
    // A yaw of zero faces along z, which is down the map.
    const ImVec2 at = view(you->position.x, you->position.z);
    const ImVec2 ahead = view.Way(std::sin(you->yaw), std::cos(you->yaw));
    const ImVec2 side{-ahead.y, ahead.x};
    const auto point = [&](float across, float along) {
      return ImVec2{at.x + Ui(along * ahead.x + across * side.x),
                    at.y + Ui(along * ahead.y + across * side.y)};
    };
    const ImVec2 tip = point(0.0f, 9.0f);
    const ImVec2 left = point(-6.0f, -6.0f);
    const ImVec2 notch = point(0.0f, -2.5f);
    const ImVec2 right = point(6.0f, -6.0f);
    draw->AddTriangleFilled(tip, left, notch, colors::kGold);
    draw->AddTriangleFilled(tip, notch, right, colors::kGoldDark);
    draw->PathLineTo(tip);
    draw->PathLineTo(right);
    draw->PathLineTo(notch);
    draw->PathLineTo(left);
    draw->PathStroke(kOutline, ImDrawFlags_Closed, Ui(1.5f));
  }
}

// The texture, as a disc of the radius round centre on the screen, showing
// the land round the place, a distance of around from it to the rim, turned
// as the view is.
void DrawDisc(ImDrawList* draw, ImTextureID texture, float half,
              const View& view, float radius, float around) {
  constexpr int kSegments = 72;
  const float size = 2.0f * half;
  const ImVec2 middle{(view.centre.x + half) / size,
                      (view.centre.z + half) / size};
  draw->PushTexture(texture);
  draw->PrimReserve(kSegments * 3, kSegments + 1);
  const unsigned int first = draw->_VtxCurrentIdx;
  draw->PrimWriteVtx(view.origin, middle, IM_COL32_WHITE);
  for (int i = 0; i < kSegments; ++i) {
    const float angle = 2.0f * kPi * static_cast<float>(i) / kSegments;
    const float across = std::cos(angle);
    const float down = std::sin(angle);
    // The way on the ground that this way on the screen shows: the view's turn
    // the other way round.
    const float c = std::cos(view.turn);
    const float s = std::sin(view.turn);
    const float x = across * c - down * s;
    const float z = across * s + down * c;
    draw->PrimWriteVtx(
        {view.origin.x + across * radius, view.origin.y + down * radius},
        {middle.x + x * around / size, middle.y + z * around / size},
        IM_COL32_WHITE);
  }
  for (int i = 0; i < kSegments; ++i) {
    draw->PrimWriteIdx(static_cast<ImDrawIdx>(first));
    draw->PrimWriteIdx(static_cast<ImDrawIdx>(first + 1 + i));
    draw->PrimWriteIdx(static_cast<ImDrawIdx>(first + 1 + (i + 1) % kSegments));
  }
  draw->PopTexture();
}

// A round button on the minimap's ring, with an icon.
bool RingButton(const char* id, ImVec2 centre, const char* icon) {
  const float radius = Ui(11.0f);
  ImGui::SetCursorScreenPos({centre.x - radius, centre.y - radius});
  const bool clicked = ImGui::InvisibleButton(id, {radius * 2, radius * 2});
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddCircleFilled(centre, radius + Ui(1.5f), kOutline);
  draw->AddCircleFilled(centre, radius,
                        hovered ? colors::kCrimson : colors::kWine);
  draw->AddCircle(centre, radius, hovered ? colors::kGold : colors::kBrass, 0,
                  Ui(1.5f));
  ImFont* font = GetFonts().text;
  const float size = Ui(12.0f);
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, icon);
  draw->AddText(font, size,
                {centre.x - extent.x / 2.0f, centre.y - extent.y / 2.0f},
                hovered ? colors::kGoldLight : colors::kPaper, icon);
  return clicked;
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

void LandMap::set_zoom(int zoom) {
  zoom_ = std::clamp(zoom, 0, static_cast<int>(kZooms.size()) - 1);
}

bool LandMap::DrawMinimap(const std::vector<Figure>& figures, uint32_t target,
                          std::optional<Vector3> destination, float heading,
                          MinimapTurn turn) {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (texture_.id == 0 || you == figures.end()) {
    bottom_ = 0.0f;
    return false;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float radius = Ui(kRadius);
  const float ring = Ui(kRing);
  const float outer = radius + ring;
  const float banner = Ui(26.0f);
  const ImVec2 size{2.0f * outer + Ui(8.0f), 2.0f * outer + banner + Ui(36.0f)};
  ImGui::SetNextWindowPos({screen.x - Ui(kMargin), Ui(kMargin)},
                          ImGuiCond_Always, {1.0f, 0.0f});
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
  ImGui::Begin("Minimap", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 corner = ImGui::GetWindowPos();
  const ImVec2 centre{corner.x + size.x / 2.0f,
                      corner.y + banner + Ui(6.0f) + outer};
  bottom_ = corner.y + size.y;

  // The land round you, in a disc, with a shade inside its rim.
  const float around = kZooms[zoom_];
  const View view{.centre = {you->position.x, you->position.z},
                  .origin = centre,
                  .scale = radius / around,
                  .turn = turn == MinimapTurn::kWithCamera ? heading : 0.0f};
  draw->AddCircleFilled(centre, radius, colors::kNight);
  DrawDisc(draw, static_cast<ImTextureID>(texture_.id), half_, view, radius,
           around);
  for (int i = 0; i < 4; ++i) {
    draw->AddCircle(centre, radius - Ui(2.0f + 2.5f * i),
                    Faded(IM_COL32_BLACK, 0.22f - 0.05f * i), 0, Ui(3.0f));
  }
  DrawMarkers(draw, view, land_, figures, target, destination, radius);

  // The ring: brass, dark at its edges, lit from the top left.
  draw->AddCircle(centre, radius + ring / 2.0f, colors::kBrass, 0, ring);
  draw->PathArcTo(centre, radius + ring / 2.0f, kPi * 1.0f, kPi * 1.6f, 24);
  draw->PathStroke(Faded(colors::kGoldLight, 0.35f), 0, ring * 0.45f);
  draw->AddCircle(centre, outer, colors::kNight, 0, Ui(1.6f));
  draw->AddCircle(centre, radius, Faded(colors::kNight, 0.9f), 0, Ui(1.6f));
  // Notches every quarter, from north, and north's own gem with its letter.
  for (int quarter = 0; quarter < 4; ++quarter) {
    const float bearing = static_cast<float>(quarter) * kPi / 2.0f - view.turn;
    const ImVec2 way{std::sin(bearing), -std::cos(bearing)};
    if (quarter == 0) {
      const ImVec2 at{centre.x + way.x * (radius + ring / 2.0f),
                      centre.y + way.y * (radius + ring / 2.0f)};
      DrawDiamond(draw, at, Ui(10.0f), colors::kWine, colors::kGold);
      ImFont* font = GetFonts().title;
      const float letter = Ui(13.0f);
      const ImVec2 extent = font->CalcTextSizeA(letter, FLT_MAX, 0.0f, "N");
      draw->AddText(font, letter,
                    {at.x - extent.x / 2.0f, at.y - extent.y / 2.0f},
                    colors::kGoldLight, "N");
      continue;
    }
    draw->AddLine({centre.x + way.x * radius, centre.y + way.y * radius},
                  {centre.x + way.x * outer, centre.y + way.y * outer},
                  colors::kNight, Ui(2.0f));
  }

  // The land's name, on a plate over the ring.
  ImFont* title = GetFonts().title;
  const std::string name = absl::StrCat("The ", kLands[land_].name);
  const float name_size = Ui(14.0f);
  const ImVec2 extent =
      title->CalcTextSizeA(name_size, FLT_MAX, 0.0f, name.c_str());
  const ImVec2 plate_min{centre.x - extent.x / 2.0f - Ui(16.0f), corner.y};
  const ImVec2 plate_max{centre.x + extent.x / 2.0f + Ui(16.0f),
                         corner.y + banner};
  DrawPanel(draw, plate_min, plate_max, 0.98f);
  DrawGradientText(draw, title, name_size,
                   {centre.x - extent.x / 2.0f,
                    (plate_min.y + plate_max.y - extent.y) / 2.0f},
                   colors::kGoldLight, colors::kGoldDark, name.c_str());

  // Where you are, in metres, under the ring.
  const std::string place =
      absl::StrCat(static_cast<int>(std::lround(you->position.x)), ", ",
                   static_cast<int>(std::lround(you->position.z)));
  ImFont* bold = GetFonts().bold;
  const float place_size = Ui(13.0f);
  const ImVec2 place_extent =
      bold->CalcTextSizeA(place_size, FLT_MAX, 0.0f, place.c_str());
  DrawShadowedText(
      draw, bold, place_size,
      {centre.x - place_extent.x / 2.0f, centre.y + outer + Ui(4.0f)},
      colors::kMuted, place.c_str());

  // Zoom in and out, and the map, on the ring's lower half.
  const auto on_ring = [&](float degrees) {
    const float angle = degrees * kPi / 180.0f;
    return ImVec2{centre.x + std::cos(angle) * (outer + Ui(2.0f)),
                  centre.y + std::sin(angle) * (outer + Ui(2.0f))};
  };
  if (RingButton("##in", on_ring(128.0f), ICON_FA_PLUS)) {
    set_zoom(zoom_ - 1);
  }
  if (RingButton("##out", on_ring(150.0f), ICON_FA_MINUS)) {
    set_zoom(zoom_ + 1);
  }
  const bool map = RingButton("##map", on_ring(45.0f), ICON_FA_MAP);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Map (M)");
  }
  // Scrolling over the disc zooms, as it would a map.
  ImGui::SetCursorScreenPos({centre.x - radius, centre.y - radius});
  ImGui::InvisibleButton("##disc", {radius * 2.0f, radius * 2.0f});
  if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
    set_zoom(zoom_ + (ImGui::GetIO().MouseWheel > 0.0f ? -1 : 1));
  }
  ImGui::End();
  return map;
}

std::optional<Place> LandMap::DrawMap(const std::vector<Figure>& figures,
                                      uint32_t target,
                                      std::optional<Vector3> destination,
                                      bool* open) const {
  if (texture_.id == 0 || !*open) {
    return std::nullopt;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float size = std::min(screen.x, screen.y) * 0.72f;
  ImGui::SetNextWindowPos({screen.x / 2, screen.y / 2}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  const std::string title = std::string("The ") + kLands[land_].name + "###Map";
  BeginPanel(title.c_str(), open,
             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
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
  draw->AddRect(min, max, colors::kBrass, 0.0f, 0, Ui(1.5f));
  draw->PushClipRect(min, max, true);
  DrawMarkers(draw, view, land_, figures, target, destination);
  draw->PopClipRect();
  ImGui::TextDisabled("Click somewhere to walk there. M closes the map.");
  EndPanel();
  return clicked;
}

}  // namespace nanommo
