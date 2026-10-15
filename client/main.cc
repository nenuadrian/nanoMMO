#include <imgui.h>
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
#include "client/atmosphere.h"
#include "client/camera.h"
#include "client/character_screen.h"
#include "client/chat_window.h"
#include "client/connection.h"
#include "client/gate.h"
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
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace {

constexpr uint16_t kLoginPort = 7778;

void ListWorlds(const std::string& host, asio::ssl::context& tls,
                const nanommo::protocol::LoginRequest& request,
                nanommo::LoginScreen& screen) {
  absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn(host, kLoginPort, tls, request);
  if (!reply.ok()) {
    screen.ShowError(
        absl::StrCat("Couldn't reach the server: ", reply.status().message()));
    return;
  }
  if (!reply->ok()) {
    screen.ShowError(reply->error());
    return;
  }
  screen.ShowWorlds({reply->worlds().begin(), reply->worlds().end()});
}

std::unique_ptr<nanommo::Connection> Enter(
    const std::string& host, const nanommo::protocol::WorldListing& world,
    bool udp, asio::ssl::context& tls,
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
  std::cout << "nanommo client: logged in as " << request.username()
            << ", entering " << world.name() << '\n';
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> connection_or =
      nanommo::Connection::Create(world.host(),
                                  static_cast<uint16_t>(world.port()), tls,
                                  reply.token(), udp);
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
  nanommo::CharacterScreen character_screen;
  nanommo::ChatWindow chat_window;
  nanommo::LoginScreen login_screen;
  nanommo::Renderer renderer;
  nanommo::Animator animator;
  std::optional<nanommo::Terrain> terrain;
  int land = 0;
  nanommo::FollowCamera camera;

  std::unique_ptr<nanommo::Connection> connection;
  nanommo::protocol::Input sent;
  int nearby = 0;

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
      if (world.players_size() != nearby) {
        nearby = world.players_size();
        std::cout << "nanommo client: we're player " << world.you() << ", "
                  << nearby << " nearby" << '\n';
      }
      // Each land has a seed of its own, so a new seed is a new land.
      if (!terrain || terrain->seed() != world.seed()) {
        // A land this client doesn't know, from a newer server, looks like
        // the Meadows.
        land = world.land() < nanommo::kLands.size()
                   ? static_cast<int>(world.land())
                   : 0;
        terrain.emplace(world.seed());
        renderer.set_terrain(*terrain, nanommo::kLooks[land]);
        renderer.set_cubes(nanommo::GateCubes(*terrain, land));
        std::cout << "nanommo client: in the " << nanommo::kLands[land].name
                  << '\n';
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
      // C is ours, unless a text field has the typing.
      if (IsKeyPressed(KEY_C) && !ImGui::GetIO().WantTextInput) {
        character_screen.Toggle();
      }
      character_screen.Render(renderer);
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
      if (const std::optional<std::string> line =
              chat_window.Build(connection->chat())) {
        // With what we sent last, so that we keep moving as we were.
        nanommo::protocol::Input say = sent;
        say.set_say(*line);
        connection->Send(say);
      }
      character_screen.Build(renderer.figures(), connection->world(),
                             nanommo::kLands[land].name);
      if (menu.Build(*window, *renderer.mutable_atmosphere(),
                     nanommo::kLooks[land].atmosphere,
                     character_screen.mutable_visible())) {
        connection.reset();
      }
    } else if (const auto request = login_screen.Build()) {
      if (request->list_worlds()) {
        ListWorlds(host, *tls, *request, login_screen);
      } else {
        connection = Enter(host, *login_screen.world(), transport == "udp",
                           *tls, *request, login_screen);
        sent.Clear();
        nearby = 0;
      }
    }
    gui.End();
    EndDrawing();
  }
  return 0;
}
