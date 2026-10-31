#include "client/cursor.h"

#include <imgui.h>
#include <raylib.h>

namespace nanommo {

namespace {

int GuiCursor() {
  switch (ImGui::GetMouseCursor()) {
    case ImGuiMouseCursor_TextInput:
      return MOUSE_CURSOR_IBEAM;
    case ImGuiMouseCursor_Hand:
      return MOUSE_CURSOR_POINTING_HAND;
    case ImGuiMouseCursor_ResizeAll:
      return MOUSE_CURSOR_RESIZE_ALL;
    case ImGuiMouseCursor_ResizeEW:
      return MOUSE_CURSOR_RESIZE_EW;
    case ImGuiMouseCursor_ResizeNS:
      return MOUSE_CURSOR_RESIZE_NS;
    case ImGuiMouseCursor_ResizeNESW:
      return MOUSE_CURSOR_RESIZE_NESW;
    case ImGuiMouseCursor_ResizeNWSE:
      return MOUSE_CURSOR_RESIZE_NWSE;
    case ImGuiMouseCursor_NotAllowed:
      return MOUSE_CURSOR_NOT_ALLOWED;
    default:
      return MOUSE_CURSOR_DEFAULT;
  }
}

}  // namespace

void UpdateCursor(Pointing pointing) {
  int cursor = MOUSE_CURSOR_DEFAULT;
  if (ImGui::GetIO().WantCaptureMouse) {
    cursor = GuiCursor();
  } else if (pointing == Pointing::kNpc) {
    cursor = MOUSE_CURSOR_POINTING_HAND;
  } else if (pointing == Pointing::kMonster) {
    cursor = MOUSE_CURSOR_CROSSHAIR;
  }
  // Each change asks the window system for a cursor, so only on a change.
  static int shown = -1;
  if (cursor != shown) {
    SetMouseCursor(cursor);
    shown = cursor;
  }
}

}  // namespace nanommo
