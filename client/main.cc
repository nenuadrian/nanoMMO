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
#include "client/audio.h"
#include "client/bag_window.h"
#include "client/camera.h"
#include "client/character_screen.h"
#include "client/chat_window.h"
#include "client/connection.h"
#include "client/gate.h"
#include "client/gui.h"
#include "client/hotbar.h"
#include "client/input.h"
#include "client/jukebox.h"
#include "client/login_screen.h"
#include "client/menu.h"
#include "client/nameplates.h"
#include "client/picking.h"
#include "client/player_frame.h"
#include "client/postprocess.h"
#include "client/quest_window.h"
#include "client/renderer.h"
#include "client/settings.h"
#include "client/sound.h"
#include "client/talk_window.h"
#include "client/trainer_window.h"
#include "client/window.h"
#include "content/content.h"
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

  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(nanommo::ContentFolder());
  if (!content.ok()) {
    std::cerr << "nanommo client: " << content.status() << "\n";
    return 1;
  }
  const uint64_t content_hash = nanommo::ClientHash(*content);
  std::cout << "nanommo client: content "
            << absl::StrCat(absl::Hex(content_hash, absl::kZeroPad16)) << '\n';

  absl::StatusOr<std::unique_ptr<nanommo::Window>> window_or =
      nanommo::Window::Create("nanoMMO", 1280, 720);
  if (!window_or.ok()) {
    std::cerr << "nanommo client: " << window_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Window> window = *std::move(window_or);
  // Without a sound card, as in a container, the game plays on in silence.
  std::unique_ptr<nanommo::AudioDevice> audio;
  std::unique_ptr<nanommo::Track> music;
  nanommo::Jukebox jukebox;
  const std::string settings_path = nanommo::SettingsPath();
  nanommo::Settings settings;
  if (absl::StatusOr<nanommo::Settings> loaded =
          nanommo::LoadSettings(settings_path);
      loaded.ok()) {
    settings = *loaded;
  } else {
    std::cerr << "nanommo client: " << loaded.status() << "\n";
  }
  nanommo::Settings saved = settings;
  if (absl::StatusOr<std::unique_ptr<nanommo::AudioDevice>> audio_or =
          nanommo::AudioDevice::Create();
      audio_or.ok()) {
    audio = *std::move(audio_or);
    absl::StatusOr<std::unique_ptr<nanommo::Track>> music_or =
        nanommo::Track::Load(*audio,
                             nanommo::MusicFolder() + "/background.mp3");
    if (music_or.ok()) {
      music = *std::move(music_or);
    } else {
      std::cerr << "nanommo client: " << music_or.status() << "\n";
    }
  } else {
    std::cerr << "nanommo client: " << audio_or.status() << "\n";
  }
  nanommo::Gui gui;
  nanommo::Menu menu;
  nanommo::CharacterScreen character_screen(content->items);
  nanommo::BagWindow bag_window(content->items);
  nanommo::ChatWindow chat_window;
  nanommo::TalkWindow talk_window;
  nanommo::TrainerWindow trainer_window(content->skills);
  nanommo::QuestWindow quest_window(*content);
  nanommo::Hotbar hotbar(content->skills);
  nanommo::LoginScreen login_screen(content_hash);
  nanommo::Renderer renderer(*content);
  nanommo::PostProcess post;
  nanommo::Animator animator(*content);
  std::optional<nanommo::Terrain> terrain;
  int land = 0;
  nanommo::FollowCamera camera;

  std::unique_ptr<nanommo::Connection> connection;
  // The world's next instance, while we move to it.
  std::unique_ptr<nanommo::Connection> next;
  nanommo::protocol::Input sent;
  int nearby = 0;

  while (!window->ShouldClose()) {
    if (connection) {
      connection->Poll();
      if (const std::optional<nanommo::protocol::Move>& move =
              connection->move();
          move && !next) {
        absl::StatusOr<std::unique_ptr<nanommo::Connection>> next_or =
            nanommo::Connection::Create(
                move->host(), static_cast<uint16_t>(move->port()), *tls,
                move->token(), transport == "udp");
        if (next_or.ok()) {
          next = *std::move(next_or);
          std::cout << "nanommo client: moving to " << move->host() << ":"
                    << move->port() << '\n';
        } else {
          std::cerr << "nanommo client: " << next_or.status() << "\n";
          // The instance we're on has stopped ticking, so there's nothing left
          // to play here.
          connection.reset();
          login_screen.ShowError("Lost the connection to the server.");
        }
      }
      // We go over once the next instance has sent us our world, so that the
      // world stays on the screen.
      if (next) {
        next->Poll();
        if (next->world().you() != 0) {
          connection = std::exchange(next, nullptr);
          sent.Clear();
        } else if (!next->connected()) {
          next.reset();
          connection.reset();
          login_screen.ShowError("Lost the connection to the server.");
        }
      }
      if (connection && !next && !connection->connected()) {
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
                      nanommo::CameraTilt(), nanommo::CameraZoom(), *terrain);
        renderer.set_camera(camera.camera());
      }
      nanommo::MarkQuestGivers(figures, *content, connection->quests());
      renderer.set_figures(std::move(figures));
      if (nanommo::ClickedWorld()) {
        const Ray ray =
            GetScreenToWorldRay(GetMousePosition(), camera.camera());
        if (const nanommo::Figure* npc =
                nanommo::PickNpc(renderer.figures(), ray)) {
          if (npc->talks) {
            talk_window.Open(npc->id, npc->name);
          } else if (!npc->quests.empty()) {
            quest_window.Open(npc->id, npc->name, npc->quests);
          } else {
            trainer_window.Open(npc->id, npc->name, npc->teaches);
          }
        }
      }
      if (IsKeyPressed(KEY_C) && !nanommo::Typing()) {
        character_screen.Toggle();
      }
      if (IsKeyPressed(KEY_I) && !nanommo::Typing()) {
        bag_window.Toggle();
      }
      character_screen.Render(renderer);
    }

    // The world goes into a frame of its own, which the screen gets from there.
    if (connection) {
      renderer.set_cel_shading(settings.graphics.cel_shading);
      post.Begin(settings.graphics);
      renderer.Draw();
      post.End();
    }
    BeginDrawing();
    if (connection) {
      post.Draw();
    } else {
      renderer.Clear();
    }
    gui.Begin();
    if (connection) {
      nanommo::DrawNameplates(renderer.figures(), renderer.camera());
      nanommo::DrawPlayerFrame(renderer.figures(), connection->world());
      nanommo::DrawQuestTracker(*content, connection->quests());
      if (const std::optional<uint32_t> skill =
              hotbar.Build(renderer.figures(), connection->world())) {
        nanommo::protocol::Input use = sent;
        use.set_skill(*skill);
        connection->Send(use);
      }
      if (const std::optional<std::string> line =
              chat_window.Build(connection->chat())) {
        // With what we sent last, so that we keep moving as we were.
        nanommo::protocol::Input say = sent;
        say.set_say(*line);
        connection->Send(say);
      }
      if (const std::optional<nanommo::protocol::Talk> talk =
              talk_window.Build()) {
        nanommo::protocol::Input ask = sent;
        *ask.mutable_talk() = *talk;
        connection->Send(ask);
      }
      if (const std::optional<nanommo::protocol::Learn> learn =
              trainer_window.Build(renderer.figures(), connection->world())) {
        nanommo::protocol::Input ask = sent;
        *ask.mutable_learn() = *learn;
        connection->Send(ask);
      }
      if (const std::optional<nanommo::protocol::Input> quest =
              quest_window.Build(connection->quests())) {
        nanommo::protocol::Input ask = sent;
        ask.MergeFrom(*quest);
        connection->Send(ask);
      }
      // What the player does with their items, in either window, goes with
      // what we sent last, like a line of chat.
      std::optional<nanommo::protocol::ItemAction> item =
          character_screen.Build(renderer.figures(), connection->world(),
                                 nanommo::kLands[land].name,
                                 connection->inventory());
      if (std::optional<nanommo::protocol::ItemAction> in_bag =
              bag_window.Build(connection->inventory())) {
        item = std::move(in_bag);
      }
      if (item) {
        nanommo::protocol::Input ask = sent;
        *ask.mutable_item() = *item;
        connection->Send(ask);
      }
      if (menu.Build(*window, settings, *renderer.mutable_atmosphere(),
                     nanommo::kLooks[land].atmosphere,
                     character_screen.mutable_visible(),
                     bag_window.mutable_visible())) {
        connection.reset();
      }
    } else if (const auto request = login_screen.Build()) {
      if (request->list_worlds()) {
        ListWorlds(host, *tls, *request, login_screen);
      } else if (const std::optional<nanommo::protocol::WorldListing>& world =
                     login_screen.world()) {
        connection = Enter(host, *world, transport == "udp", *tls, *request,
                           login_screen);
        sent.Clear();
        nearby = 0;
      }
    }
    // Once a slider lets go, so that a drag across one writes once.
    if (settings != saved && !ImGui::IsAnyItemActive()) {
      if (const absl::Status written =
              nanommo::SaveSettings(settings_path, settings);
          !written.ok()) {
        std::cerr << "nanommo client: " << written << "\n";
      }
      saved = settings;
    }
    gui.End();
    EndDrawing();

    if (music) {
      switch (jukebox.Update(GetFrameTime(), connection != nullptr,
                             music->playing())) {
        case nanommo::Jukebox::Cue::kStart:
          music->Play();
          break;
        case nanommo::Jukebox::Cue::kStop:
          music->Stop();
          break;
        case nanommo::Jukebox::Cue::kNothing:
          break;
      }
      music->SetVolume(nanommo::Gain(settings.sound.music) * jukebox.volume());
      music->Update();
    }
    // The master volume covers each sound the game makes, so far the music.
    if (audio) {
      SetMasterVolume(
          settings.sound.muted ? 0.0f : nanommo::Gain(settings.sound.master));
    }
  }
  return 0;
}
