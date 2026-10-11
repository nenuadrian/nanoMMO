#include <raylib.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "client/connection.h"
#include "client/gui.h"
#include "client/input.h"
#include "client/menu.h"
#include "client/renderer.h"
#include "client/scene.h"
#include "client/window.h"

int main(int argc, char* argv[]) {
  const std::string host = argc > 1 ? argv[1] : "localhost";
  constexpr uint16_t kPort = 7777;
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> connection_or =
      nanommo::Connection::Create(host, kPort);
  if (!connection_or.ok()) {
    std::cerr << "nanommo client: " << connection_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Connection> connection =
      *std::move(connection_or);
  std::cout << "nanommo client: connected to " << host << ":" << kPort << '\n';

  absl::StatusOr<std::unique_ptr<nanommo::Window>> window_or =
      nanommo::Window::Create("nanoMMO", 1280, 720);
  if (!window_or.ok()) {
    std::cerr << "nanommo client: " << window_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Window> window = *std::move(window_or);
  nanommo::Gui gui;
  nanommo::Menu menu;
  nanommo::Renderer renderer;

  nanommo::protocol::Input sent;
  int online = 0;

  while (!window->ShouldClose()) {
    connection->Poll();
    if (!connection->connected()) {
      std::cerr << "nanommo client: lost the connection to the server\n";
      return 1;
    }

    const Vector3 direction = nanommo::Movement();
    if (direction.x != sent.x() || direction.z != sent.z()) {
      sent.set_x(direction.x);
      sent.set_z(direction.z);
      connection->Send(sent);
    }

    const nanommo::protocol::World& world = connection->world();
    if (world.players_size() != online) {
      online = world.players_size();
      std::cout << "nanommo client: we're player " << world.you() << ", "
                << online << " online" << '\n';
    }
    renderer.set_cubes(nanommo::Scene(world));

    BeginDrawing();
    renderer.Draw();
    gui.Begin();
    menu.Build(*window, *renderer.mutable_background());
    gui.End();
    EndDrawing();
  }
  return 0;
}
