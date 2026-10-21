#include "client/input.h"

#include <imgui.h>

#include <numbers>

namespace nanommo {

namespace {

// Half a turn a second.
constexpr float kTurnSpeed = std::numbers::pi_v<float>;

// For each pixel the mouse moves.
constexpr float kDragTurn = 0.005f;

constexpr float kZoomStep = 0.1f;

}  // namespace

bool Typing() { return ImGui::GetIO().WantTextInput; }

Vector3 Movement() {
  const auto down = [](int key, int arrow) {
    return IsKeyDown(key) || IsKeyDown(arrow);
  };

  Vector3 direction{0.0f, 0.0f, 0.0f};
  if (Typing()) {
    return direction;
  }
  if (down(KEY_W, KEY_UP)) {
    direction.z -= 1.0f;
  }
  if (down(KEY_S, KEY_DOWN)) {
    direction.z += 1.0f;
  }
  if (down(KEY_A, KEY_LEFT)) {
    direction.x -= 1.0f;
  }
  if (down(KEY_D, KEY_RIGHT)) {
    direction.x += 1.0f;
  }
  return direction;
}

bool Running() {
  return !Typing() && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
}

bool Attacking() { return !Typing() && IsKeyDown(KEY_SPACE); }

bool ClickedWorld() {
  return !ImGui::GetIO().WantCaptureMouse &&
         IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

float CameraTurn(float seconds) {
  float turn = 0.0f;
  if (!Typing() && IsKeyDown(KEY_Q)) {
    turn += kTurnSpeed * seconds;
  }
  if (!Typing() && IsKeyDown(KEY_E)) {
    turn -= kTurnSpeed * seconds;
  }
  // The mouse is the GUI's while it's over one of its windows.
  if (!ImGui::GetIO().WantCaptureMouse &&
      IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    turn -= GetMouseDelta().x * kDragTurn;
  }
  return turn;
}

float CameraTilt() {
  if (ImGui::GetIO().WantCaptureMouse ||
      !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    return 0.0f;
  }
  return GetMouseDelta().y * kDragTurn;
}

float CameraZoom() {
  if (ImGui::GetIO().WantCaptureMouse) {
    return 0.0f;
  }
  return GetMouseWheelMove() * kZoomStep;
}

}  // namespace nanommo
