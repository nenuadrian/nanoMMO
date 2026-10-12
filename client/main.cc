#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "asio/ssl/context.hpp"
#include "client/animator.h"
#include "client/camera.h"
#include "client/connection.h"
#include "client/gui.h"
#include "client/input.h"
#include "client/login_screen.h"
#include "client/menu.h"
#include "client/nameplates.h"
#include "client/player_frame.h"
#include "client/renderer.h"
#include "client/window.h"
#include "protocol/login_client.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "terrain/terrain.h"

namespace {

constexpr uint16_t kGamePort = 7777;
constexpr uint16_t kLoginPort = 7778;

std::unique_ptr<nanommo::Connection> Enter(
    const std::string& host, bool udp, asio::ssl::context& tls,
    const nanommo::protocol::LoginRequest& request,
    nanommo::LoginScreen& screen) {
  absl::StatusOr<nanommo::protocol::LoginReply> reply_or =
      nanommo::LogIn(host, kLoginPort, tls, request);
  if (!reply_or.ok()) {
    screen.ShowError(absl::StrCat("Couldn't reach the server: ",
                                  reply_or.status().message()));
    return nullptr;
  }
  const nanommo::protocol::LoginReply& reply = *reply_or;
  if (!reply.ok()) {
    screen.ShowError(reply.error());
    return nullptr;
  }
  std::cout << "nanommo client: logged in as " << request.username() << '\n';
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> connection_or =
      nanommo::Connection::Create(host, kGamePort, tls, reply.token(), udp);
  if (!connection_or.ok()) {
    screen.ShowError(absl::StrCat("Couldn't reach the server: ",
                                  connection_or.status().message()));
    return nullptr;
  }
  return *std::move(connection_or);
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::string host = argc > 1 ? argv[1] : "localhost";
  const std::string transport = argc > 2 ? argv[2] : "udp";
  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(nanommo::Setting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    std::cerr << "nanommo client: " << tls.status() << "\n";
    return 1;
  }

  absl::StatusOr<std::unique_ptr<nanommo::Window>> window_or =
      nanommo::Window::Create("nanoMMO", 1280, 720);
  if (!window_or.ok()) {
    std::cerr << "nanommo client: " << window_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Window> window = *std::move(window_or);
  nanommo::Gui gui;
  nanommo::Menu menu;
  nanommo::LoginScreen login_screen;
  nanommo::Renderer renderer;
  nanommo::Animator animator;
  std::optional<nanommo::Terrain> terrain;
  nanommo::FollowCamera camera;

  std::unique_ptr<nanommo::Connection> connection;
  nanommo::protocol::Input sent;
  int online = 0;

  while (!window->ShouldClose()) {
    if (connection) {
      connection->Poll();
      if (!connection->connected()) {
        connection.reset();
        login_screen.ShowError("Lost the connection to the server.");
      }
    }

    if (connection) {
      const Vector3 direction = camera.Relative(nanommo::Movement());
      const bool run = nanommo::Running();
      const bool attack = nanommo::Attacking();
      if (direction.x != sent.x() || direction.z != sent.z() ||
          run != sent.run() || attack != sent.attack()) {
        sent.set_x(direction.x);
        sent.set_z(direction.z);
        sent.set_run(run);
        sent.set_attack(attack);
        connection->Send(sent);
      }

      const nanommo::protocol::World& world = connection->world();
      if (world.players_size() != online) {
        online = world.players_size();
        std::cout << "nanommo client: we're player " << world.you() << ", "
                  << online << " online" << '\n';
      }
      if (!terrain || terrain->seed() != world.seed()) {
        terrain.emplace(world.seed());
        renderer.set_terrain(*terrain);
      }
      std::vector<nanommo::Figure> figures =
          animator.Update(world, GetFrameTime());
      for (nanommo::Figure& figure : figures) {
        figure.position.y =
            terrain->Height(figure.position.x, figure.position.z);
      }
      const auto you = std::ranges::find_if(figures, &nanommo::Figure::you);
      if (you != figures.end()) {
        camera.Update(you->position, nanommo::CameraTurn(GetFrameTime()),
                      nanommo::CameraZoom(), *terrain);
        renderer.set_camera(camera.camera());
      }
      renderer.set_figures(std::move(figures));
    }

    BeginDrawing();
    if (connection) {
      renderer.Draw();
    } else {
      renderer.Clear();
    }
    gui.Begin();
    if (connection) {
      nanommo::DrawNameplates(renderer.figures(), renderer.camera());
      nanommo::DrawPlayerFrame(renderer.figures(), connection->world());
      if (menu.Build(*window, *renderer.mutable_background())) {
        connection.reset();
      }
    } else if (const auto request = login_screen.Build()) {
      connection =
          Enter(host, transport == "udp", *tls, *request, login_screen);
      sent.Clear();
      online = 0;
    }
    gui.End();
    EndDrawing();
  }
  return 0;
}
