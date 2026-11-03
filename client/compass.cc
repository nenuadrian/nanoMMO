#include "client/compass.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "client/atmosphere.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Half the world, a quarter turn to each side of the way the camera looks.
constexpr float kSpan = kPi;

// In points, at a scale of one.
constexpr float kWidth = 620.0f;
constexpr float kHeight = 36.0f;
constexpr float kTop = 14.0f;

// Nearer the middle than this share of each half, a mark says how far it is.
constexpr float kNear = 0.12f;

// The compass's points, every eighth of a turn from north.
constexpr std::array<const char*, 8> kPoints = {"N", "NE", "E", "SE",
                                                "S", "SW", "W", "NW"};

// Things fade out towards the compass's ends, from three quarters of the way.
float EdgeFade(float at) {
  return std::clamp((1.0f - std::abs(at)) / 0.25f, 0.0f, 1.0f);
}

// A line along the compass's edge, which fades out at its ends.
void DrawEdge(ImDrawList* draw, float left, float right, float y) {
  const float middle = (left + right) / 2.0f;
  const float thickness = std::max(1.0f, Ui(1.0f));
  const ImU32 brass = colors::kBrass;
  draw->AddRectFilledMultiColor({left, y}, {middle, y + thickness},
                                Faded(brass, 0.0f), brass, brass,
                                Faded(brass, 0.0f));
  draw->AddRectFilledMultiColor({middle, y}, {right, y + thickness}, brass,
                                Faded(brass, 0.0f), Faded(brass, 0.0f), brass);
}

void DrawCentred(ImDrawList* draw, ImFont* font, float size, ImVec2 centre,
                 ImU32 color, const char* text) {
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  DrawShadowedText(draw, font, size,
                   {centre.x - extent.x / 2.0f, centre.y - extent.y / 2.0f},
                   color, text);
}

ImU32 SkyOf(int land) {
  const std::array<float, 3>& sky = kLooks[land].atmosphere.sky;
  const auto byte = [](float value) {
    return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  };
  return IM_COL32(byte(sky[0]), byte(sky[1]), byte(sky[2]), 255);
}

void DrawMark(ImDrawList* draw, const CompassMark& mark, ImVec2 at,
              float alpha) {
  const Fonts& fonts = GetFonts();
  switch (mark.kind) {
    case CompassMark::Kind::kTarget:
      DrawCentred(draw, fonts.text, Ui(16.0f), at, Faded(mark.color, alpha),
                  ICON_FA_SKULL);
      break;
    case CompassMark::Kind::kQuest:
    case CompassMark::Kind::kTurnIn:
      DrawCentred(draw, fonts.title, Ui(22.0f), at, Faded(mark.color, alpha),
                  mark.kind == CompassMark::Kind::kQuest ? "!" : "?");
      break;
    case CompassMark::Kind::kGate:
      draw->AddCircle(at, Ui(6.5f), Faded(IM_COL32_BLACK, 0.8f * alpha), 0,
                      Ui(4.5f));
      draw->AddCircle(at, Ui(6.5f), Faded(mark.color, alpha), 0, Ui(2.5f));
      break;
    case CompassMark::Kind::kDestination:
      DrawCentred(draw, fonts.text, Ui(15.0f), at, Faded(mark.color, alpha),
                  ICON_FA_LOCATION_DOT);
      break;
  }
}

}  // namespace

float Bearing(Vector3 from, Vector3 to) {
  return std::atan2(to.x - from.x, -(to.z - from.z));
}

float Heading(const Camera3D& camera) {
  return Bearing(camera.position, camera.target);
}

std::optional<float> CompassAt(float heading, float bearing, float span) {
  // Round from the heading the shorter way, to between minus and plus half a
  // turn.
  const float round = std::remainder(bearing - heading, 2.0f * kPi);
  if (std::abs(round) > span / 2.0f) {
    return std::nullopt;
  }
  return round / (span / 2.0f);
}

std::vector<CompassMark> CompassMarks(const std::vector<Figure>& figures,
                                      int land, uint32_t target,
                                      std::optional<Vector3> destination) {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  if (you == figures.end()) {
    return {};
  }
  const Vector3 here = you->position;
  std::vector<CompassMark> marks;
  const auto add = [&](CompassMark::Kind kind, Vector3 to, ImU32 color) {
    marks.push_back({.kind = kind,
                     .bearing = Bearing(here, to),
                     .distance = std::hypot(to.x - here.x, to.z - here.z),
                     .color = color});
  };
  for (const Gate& gate : kGates) {
    if (gate.land == land) {
      add(CompassMark::Kind::kGate, {gate.place.x, 0.0f, gate.place.z},
          SkyOf(gate.to));
    }
  }
  for (const Figure& figure : figures) {
    if (figure.npc && figure.mark == '!') {
      add(CompassMark::Kind::kQuest, figure.position, colors::kGold);
    } else if (figure.npc && figure.mark == '?') {
      add(CompassMark::Kind::kTurnIn, figure.position, colors::kGold);
    }
  }
  if (destination) {
    add(CompassMark::Kind::kDestination, *destination, colors::kGoldLight);
  }
  // Last, so that it's drawn over the rest.
  for (const Figure& figure : figures) {
    if (figure.monster && figure.id == target && target != 0 &&
        figure.to != protocol::MOTION_DEAD) {
      add(CompassMark::Kind::kTarget, figure.position, colors::kEmber);
    }
  }
  return marks;
}

void DrawCompass(float heading, const std::vector<CompassMark>& marks) {
  // Under the windows, so that one dragged over it covers it.
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const Fonts& fonts = GetFonts();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float width = std::min(Ui(kWidth), screen.x * 0.6f);
  const float height = Ui(kHeight);
  const ImVec2 min{(screen.x - width) / 2.0f, Ui(kTop)};
  const ImVec2 max{min.x + width, min.y + height};
  const float middle = (min.x + max.x) / 2.0f;
  const float half = width / 2.0f;

  // A dark band, which fades out at its ends.
  const float fade = width * 0.2f;
  const ImU32 band = Faded(colors::kNight, 0.7f);
  const ImU32 clear = Faded(colors::kNight, 0.0f);
  draw->AddRectFilledMultiColor(min, {min.x + fade, max.y}, clear, band, band,
                                clear);
  draw->AddRectFilled({min.x + fade, min.y}, {max.x - fade, max.y}, band);
  draw->AddRectFilledMultiColor({max.x - fade, min.y}, max, band, clear, clear,
                                band);
  DrawEdge(draw, min.x, max.x, min.y);
  DrawEdge(draw, min.x, max.x, max.y - std::max(1.0f, Ui(1.0f)));

  // The ticks, every five degrees, longer every fifteen, and the points, every
  // forty-five.
  for (int degrees = 0; degrees < 360; degrees += 5) {
    const std::optional<float> at =
        CompassAt(heading, static_cast<float>(degrees) * kPi / 180.0f, kSpan);
    if (!at) {
      continue;
    }
    const float x = middle + *at * half;
    const float alpha = EdgeFade(*at);
    if (degrees % 45 == 0) {
      const bool cardinal = degrees % 90 == 0;
      const ImU32 color =
          degrees == 0 ? colors::kEmber
                       : (cardinal ? colors::kGoldLight : colors::kMuted);
      DrawCentred(draw, fonts.title, cardinal ? Ui(22.0f) : Ui(14.0f),
                  {x, (min.y + max.y) / 2.0f}, Faded(color, alpha),
                  kPoints[degrees / 45]);
    } else {
      const float tall = degrees % 15 == 0 ? Ui(9.0f) : Ui(5.0f);
      const ImU32 color =
          degrees % 15 == 0 ? colors::kGoldDark : colors::kFaint;
      draw->AddLine({x, max.y - Ui(3.0f) - tall}, {x, max.y - Ui(3.0f)},
                    Faded(color, alpha), std::max(1.0f, Ui(1.2f)));
    }
  }

  // The marks, and how far the one nearest the middle is, if it's near.
  const CompassMark* nearest = nullptr;
  float nearest_at = kNear;
  for (const CompassMark& mark : marks) {
    if (const std::optional<float> at = CompassAt(heading, mark.bearing, kSpan);
        at && std::abs(*at) < nearest_at) {
      nearest = &mark;
      nearest_at = std::abs(*at);
    }
  }
  for (const CompassMark& mark : marks) {
    const std::optional<float> at = CompassAt(heading, mark.bearing, kSpan);
    const bool kept = mark.kind == CompassMark::Kind::kTarget ||
                      mark.kind == CompassMark::Kind::kQuest ||
                      mark.kind == CompassMark::Kind::kTurnIn;
    if (!at && !kept) {
      continue;
    }
    // Behind you, it waits at the end it's nearer.
    const float side =
        at ? *at
           : (std::remainder(mark.bearing - heading, 2.0f * kPi) < 0.0f ? -1.0f
                                                                        : 1.0f);
    const float x = middle + side * (half - (at ? 0.0f : Ui(10.0f)));
    const float alpha = at ? std::max(EdgeFade(*at), 0.0f) : 0.8f;
    if (alpha <= 0.0f) {
      continue;
    }
    DrawMark(draw, mark, {x, (min.y + max.y) / 2.0f}, alpha);
    if (!at) {
      // An arrow out past the end, the way to turn.
      const float tip = side < 0.0f ? min.x - Ui(2.0f) : max.x + Ui(2.0f);
      const float back = tip - side * Ui(7.0f);
      const float y = (min.y + max.y) / 2.0f;
      draw->AddTriangleFilled({tip, y}, {back, y - Ui(5.0f)},
                              {back, y + Ui(5.0f)}, Faded(mark.color, 0.8f));
    } else if (&mark == nearest) {
      const std::string distance =
          std::to_string(static_cast<int>(std::lround(mark.distance))) + " m";
      DrawCentred(draw, fonts.bold, Ui(13.0f), {x, max.y + Ui(9.0f)},
                  Faded(colors::kPaper, 0.9f), distance.c_str());
    }
  }

  // A thin line down the middle, and a notch at the top, where the camera
  // looks.
  draw->AddLine({middle, min.y + Ui(2.0f)}, {middle, max.y - Ui(2.0f)},
                Faded(colors::kGold, 0.35f), std::max(1.0f, Ui(1.0f)));
  const float notch = Ui(7.0f);
  draw->AddTriangleFilled({middle - notch, min.y - Ui(1.0f)},
                          {middle + notch, min.y - Ui(1.0f)},
                          {middle, min.y + notch}, colors::kGold);
}

}  // namespace nanommo
