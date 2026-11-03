#include <imgui.h>
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
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
#include "client/combat_text.h"
#include "client/compass.h"
#include "client/connection.h"
#include "client/cursor.h"
#include "client/death_screen.h"
#include "client/foley.h"
#include "client/gate.h"
#include "client/gui.h"
#include "client/hotbar.h"
#include "client/input.h"
#include "client/jukebox.h"
#include "client/loading.h"
#include "client/login_screen.h"
#include "client/map.h"
#include "client/menu.h"
#include "client/nameplates.h"
#include "client/picking.h"
#include "client/player_frame.h"
#include "client/postprocess.h"
#include "client/quest_window.h"
#include "client/renderer.h"
#include "client/settings.h"
#include "client/sound.h"
#include "client/soundboard.h"
#include "client/soundscape.h"
#include "client/talk_window.h"
#include "client/trainer_window.h"
#include "client/window.h"
#include "content/content.h"
#include "protocol/login_client.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "protocol/version.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace {

constexpr uint16_t kLoginPort = 7778;

// In metres from where we walk to by ourselves: there.
constexpr float kThere = 0.6f;

// In seconds: a walk we asked for that stands still after this has ended, as
// we got there, or something stopped us.
constexpr float kSetOff = 0.5f;

// The login server's answer to the request, which the screen shows, or why
// there's none.
std::optional<nanommo::protocol::LoginReply> Ask(
    const std::string& host, asio::ssl::context& tls,
    const nanommo::protocol::LoginRequest& request,
    nanommo::LoginScreen& screen) {
  absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn(host, kLoginPort, tls, request);
  if (!reply.ok()) {
    screen.ShowError(
        absl::StrCat("Couldn't reach the server: ", reply.status().message()));
    return std::nullopt;
  }
  return *std::move(reply);
}

// Into the world, as the character the token's for.
std::unique_ptr<nanommo::Connection> Enter(
    const nanommo::protocol::WorldListing& world, bool udp,
    asio::ssl::context& tls, const std::string& token,
    nanommo::LoginScreen& screen) {
  std::cout << "nanommo client: entering " << world.name() << " as "
            << screen.character_name() << '\n';
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> connection_or =
      nanommo::Connection::Create(world.host(),
                                  static_cast<uint16_t>(world.port()), tls,
                                  token, udp);
  if (!connection_or.ok()) {
    screen.ShowError(absl::StrCat("Couldn't reach the server: ",
                                  connection_or.status().message()));
    return nullptr;
  }
  return *std::move(connection_or);
}

}  // namespace

int main(int argc, char* argv[]) {
  std::cout << "nanommo client: version " << nanommo::kVersion << '\n';
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
  // The steps, the skills and the gates, made as the game starts.
  std::unique_ptr<nanommo::Soundboard> soundboard;
  nanommo::Footsteps footsteps;
  nanommo::Casts casts;
  nanommo::Passage passage;
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
    soundboard = std::make_unique<nanommo::Soundboard>(*audio, content->skills);
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
  nanommo::PlayerFrame player_frame;
  nanommo::LoginScreen login_screen(content_hash, content->items);
  nanommo::Renderer renderer(*content);
  nanommo::PostProcess post;
  nanommo::Animator animator(*content);
  nanommo::CombatText combat_text;
  nanommo::Loading loading;
  nanommo::DeathScreen death_screen;
  nanommo::LandMap land_map;
  bool map_open = false;
  std::optional<nanommo::Terrain> terrain;
  int land = 0;
  nanommo::FollowCamera camera;

  std::unique_ptr<nanommo::Connection> connection;
  // The world's next instance, while we move to it.
  std::unique_ptr<nanommo::Connection> next;
  // The token we enter the world with, once the loading screen's up.
  std::optional<std::string> entering;
  std::string world_name;
  nanommo::protocol::Input sent;
  // The monster the player picked, by its id.
  uint32_t target = 0;
  // Where we asked to walk to by ourselves, and for how long we have.
  std::optional<Vector3> destination;
  float walking_for = 0.0f;
  int nearby = 0;

  while (!window->ShouldClose()) {
    // Connecting holds up the frame, so it waits for the loading screen to be
    // on the screen first.
    if (entering && loading.covered()) {
      if (const std::optional<nanommo::protocol::WorldListing>& world =
              login_screen.world()) {
        connection =
            Enter(*world, transport == "udp", *tls, *entering, login_screen);
      }
      entering.reset();
      sent.Clear();
      target = 0;
      nearby = 0;
      // So that the land is built again, behind the loading screen.
      terrain.reset();
    }
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

    // Whether our world has come, and its land is built, to be drawn.
    bool in_world = false;
    bool dead = false;
    nanommo::Pointing pointing = nanommo::Pointing::kNothing;
    if (connection) {
      const Vector3 direction = camera.Relative(nanommo::Movement());
      const bool run = nanommo::Running();
      const bool attack = nanommo::Attacking();
      if (direction.x != sent.x() || direction.z != sent.z() ||
          run != sent.run() || attack != sent.attack() ||
          target != sent.target()) {
        sent.set_x(direction.x);
        sent.set_z(direction.z);
        sent.set_run(run);
        sent.set_attack(attack);
        sent.set_target(target);
        connection->Send(sent);
      }
      // The keys and a swing call off a walk we asked for, as on the server.
      if (direction.x != 0.0f || direction.z != 0.0f || attack) {
        destination.reset();
      }

      const nanommo::protocol::World& world = connection->world();
      if (world.players_size() != nearby) {
        nearby = world.players_size();
        std::cout << "nanommo client: we're player " << world.you() << ", "
                  << nearby << " nearby" << '\n';
      }
      // Each land has a seed of its own, so a new seed is a new land. Until
      // our world comes, we don't know which land we're in.
      const bool arrived = world.you() != 0;
      const bool new_land =
          arrived && (!terrain || terrain->seed() != world.seed());
      // Building a land holds up the frame, so it's built behind the loading
      // screen, once that's on the screen.
      if (new_land && loading.covered()) {
        // A land this client doesn't know, from a newer server, looks like
        // the Meadows.
        land = world.land() < nanommo::kLands.size()
                   ? static_cast<int>(world.land())
                   : 0;
        terrain.emplace(world.seed());
        renderer.set_terrain(*terrain, nanommo::kLooks[land]);
        renderer.set_cubes(nanommo::GateCubes(*terrain, land));
        land_map.Build(*terrain, land);
        std::cout << "nanommo client: in the " << nanommo::kLands[land].name
                  << '\n';
        // The monster we picked, and where we walked to, were the last land's.
        target = 0;
        destination.reset();
        loading.Lift();
      } else if (!arrived || new_land) {
        loading.Cover();
      } else if (loading.covering()) {
        loading.Lift();
      }
      in_world = arrived && terrain && terrain->seed() == world.seed();
    }

    // Asks the world to walk us there, around what's in the way.
    const auto walk_to = [&](float x, float z) {
      nanommo::protocol::Input walk = sent;
      walk.mutable_walk_to()->set_x(x);
      walk.mutable_walk_to()->set_z(z);
      connection->Send(walk);
      destination = Vector3{x, terrain->Height(x, z), z};
      walking_for = 0.0f;
    };

    if (in_world) {
      const nanommo::protocol::World& world = connection->world();
      std::vector<nanommo::Figure> figures =
          animator.Update(world, GetFrameTime());
      for (nanommo::Figure& figure : figures) {
        figure.position.y =
            terrain->Height(figure.position.x, figure.position.z);
      }
      const auto you = std::ranges::find_if(figures, &nanommo::Figure::you);
      dead = you != figures.end() && you->to == nanommo::protocol::MOTION_DEAD;
      if (you != figures.end()) {
        camera.Update(you->position, nanommo::CameraTurn(GetFrameTime()),
                      nanommo::CameraTilt(), nanommo::CameraZoom(), *terrain);
        renderer.set_camera(camera.camera());
      }
      const Vector3 here =
          you != figures.end() ? you->position : Vector3{0.0f, 0.0f, 0.0f};
      if (destination) {
        walking_for += GetFrameTime();
        const bool there = std::hypot(here.x - destination->x,
                                      here.z - destination->z) < kThere;
        const bool stopped = walking_for > kSetOff && you != figures.end() &&
                             you->to == nanommo::protocol::MOTION_IDLE;
        if (there || stopped || dead) {
          destination.reset();
        }
      }
      for (const nanommo::protocol::Event& event : connection->TakeEvents()) {
        combat_text.Add(event, world.you());
        if (event.kind() == nanommo::protocol::EVENT_HIT) {
          renderer.Struck(event.monster(), event.player());
        }
      }
      combat_text.Update(GetFrameTime());
      nanommo::MarkQuestGivers(figures, *content, connection->quests());
      // The monster we picked, while it's alive and near enough to be sent.
      if (std::ranges::none_of(figures, [target](const nanommo::Figure& f) {
            return f.monster && f.id == target &&
                   f.to != nanommo::protocol::MOTION_DEAD;
          })) {
        target = 0;
      }
      renderer.set_figures(std::move(figures));
      renderer.Animate(GetFrameTime());
      // We hear from where we stand, with the camera's right to our right.
      if (soundboard) {
        const Camera3D& eye = camera.camera();
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(eye.target, eye.position), eye.up));
        std::map<uint64_t, nanommo::Gait> gaits;
        std::map<uint64_t, const nanommo::Figure*> walking;
        for (const nanommo::Figure& figure : renderer.figures()) {
          if (const std::optional<nanommo::Gait> gait =
                  renderer.GaitOf(figure)) {
            gaits[nanommo::Who(figure)] = *gait;
            walking[nanommo::Who(figure)] = &figure;
          }
        }
        for (const uint64_t who : footsteps.Update(gaits)) {
          const Vector3& at = walking.at(who)->position;
          soundboard->Step(nanommo::GroundAt(*terrain, land, at.x, at.z),
                           nanommo::Hear(at, here, right));
        }
        for (const nanommo::Figure& figure : casts.Update(renderer.figures())) {
          soundboard->Skill(figure.to_skill, figure.target != 0,
                            nanommo::Hear(figure.position, here, right));
        }
      }
      // A click on someone to talk to opens their window, one on a monster
      // picks it, and one on neither lets go of what we'd picked. The cursor
      // says which a click would do.
      if (!ImGui::GetIO().WantCaptureMouse && !dead) {
        const Ray ray =
            GetScreenToWorldRay(GetMousePosition(), camera.camera());
        const nanommo::Figure* npc =
            nanommo::PickNpc(renderer.figures(), ray, here);
        const nanommo::Figure* monster =
            npc == nullptr ? nanommo::PickMonster(renderer.figures(), ray)
                           : nullptr;
        if (npc != nullptr) {
          pointing = nanommo::Pointing::kNpc;
        } else if (monster != nullptr) {
          pointing = nanommo::Pointing::kMonster;
        }
        if (nanommo::ClickedWorld()) {
          if (npc == nullptr) {
            target = monster != nullptr ? monster->id : 0;
          } else if (npc->talks) {
            talk_window.Open(npc->id, npc->name);
          } else if (!npc->quests.empty()) {
            quest_window.Open(npc->id, npc->name, npc->quests);
          } else {
            trainer_window.Open(npc->id, npc->name, npc->teaches);
          }
        }
        // A double click on the ground walks there, around what's in the way.
        if (npc == nullptr && monster == nullptr &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          if (const std::optional<Vector3> ground =
                  nanommo::PickGround(*terrain, ray);
              ground && terrain->Walkable(ground->x, ground->z)) {
            walk_to(ground->x, ground->z);
          }
        }
      }
      // Escape closes what's on top, then lets go of the monster picked, and
      // then opens the menu.
      if (IsKeyPressed(KEY_ESCAPE) && !nanommo::Typing()) {
        if (menu.settings_open()) {
          menu.CloseSettings();
        } else if (menu.open()) {
          menu.Toggle();
        } else if (map_open) {
          map_open = false;
        } else if (talk_window.open() || trainer_window.open() ||
                   quest_window.open()) {
          talk_window.Close();
          trainer_window.Close();
          quest_window.Close();
        } else if (bag_window.visible()) {
          bag_window.Toggle();
        } else if (*character_screen.mutable_visible()) {
          character_screen.Toggle();
        } else if (target != 0) {
          target = 0;
        } else {
          menu.Toggle();
        }
      }
      if (IsKeyPressed(KEY_M) && !nanommo::Typing()) {
        map_open = !map_open;
      }
      renderer.set_target(target);
      renderer.set_destination(destination);
      if (IsKeyPressed(KEY_C) && !nanommo::Typing()) {
        character_screen.Toggle();
      }
      if (IsKeyPressed(KEY_I) && !nanommo::Typing()) {
        bag_window.Toggle();
      }
      character_screen.Render(renderer);
      player_frame.Render(renderer);
    } else if (!entering) {
      // The character picked on the login screen, drawn before the frame.
      login_screen.Render(renderer);
    }

    // Before the frame is drawn, so that the frame we're back in is the dark
    // one, which hides our jump to the middle.
    death_screen.Update(GetFrameTime(), dead);

    // The world goes into a frame of its own, which the screen gets from there.
    if (in_world) {
      renderer.set_cel_shading(settings.graphics.cel_shading);
      renderer.set_clear(loading.clear());
      post.Begin(settings.graphics);
      renderer.Draw();
      post.End();
    }
    BeginDrawing();
    if (in_world) {
      post.Draw();
    } else {
      renderer.Clear();
    }
    gui.Begin(settings.ui.scale);
    if (in_world) {
      nanommo::DrawNameplates(renderer.figures(), renderer.camera());
      nanommo::DrawCombatText(combat_text, renderer.figures(),
                              renderer.camera());
      // Over the nameplates, and under the windows.
      if (death_screen.Build(dead, connection->world().respawn())) {
        nanommo::protocol::Input respawn = sent;
        respawn.set_respawn(true);
        connection->Send(respawn);
      }
      nanommo::DrawCompass(
          nanommo::Heading(renderer.camera()),
          nanommo::CompassMarks(renderer.figures(), land, target, destination));
      player_frame.Draw(renderer.figures(), connection->world());
      nanommo::DrawTargetFrame(renderer.figures(), target);
      if (const std::optional<uint32_t> skill =
              hotbar.Build(renderer.figures(), connection->world(), target)) {
        nanommo::protocol::Input use = sent;
        use.set_skill(*skill);
        connection->Send(use);
        destination.reset();
      }
      if (land_map.DrawMinimap(renderer.figures(), target, destination,
                               nanommo::Heading(renderer.camera()),
                               settings.ui.minimap_turns
                                   ? nanommo::MinimapTurn::kWithCamera
                                   : nanommo::MinimapTurn::kNorthUp)) {
        map_open = !map_open;
      }
      nanommo::DrawQuestTracker(*content, connection->quests(),
                                land_map.minimap_bottom());
      if (const std::optional<nanommo::Place> place = land_map.DrawMap(
              renderer.figures(), target, destination, &map_open);
          place && terrain->Walkable(place->x, place->z)) {
        walk_to(place->x, place->z);
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
      menu.BuildButtons(character_screen.mutable_visible(),
                        bag_window.mutable_visible(), &map_open);
      switch (menu.Build(settings, *renderer.mutable_atmosphere(),
                         nanommo::kLooks[land].atmosphere)) {
        case nanommo::Menu::Choice::kLogOut:
          connection.reset();
          break;
        case nanommo::Menu::Choice::kQuit:
          window->Close();
          break;
        case nanommo::Menu::Choice::kNothing:
          break;
      }
    } else if (!connection && !entering) {
      if (const auto request = login_screen.Build()) {
        if (const std::optional<nanommo::protocol::LoginReply> reply =
                Ask(host, *tls, *request, login_screen)) {
          // A token for the character picked: into its world, behind the
          // loading screen. Anything else, the screen shows.
          if (request->enter() && reply->ok()) {
            entering = reply->token();
            world_name = login_screen.world()->name();
            loading.Cover();
          } else {
            login_screen.Show(*reply);
          }
        }
      }
    }
    // Back on the login screen, there's nothing to cover.
    if (!connection && !entering) {
      loading = nanommo::Loading();
    }
    // While our world hasn't come, we're entering it; once it has, the
    // screen says which land it's in, in that land's sky.
    if (connection && connection->world().you() != 0) {
      const uint32_t shown = connection->world().land();
      const int arriving =
          shown < nanommo::kLands.size() ? static_cast<int>(shown) : 0;
      nanommo::DrawLoadingScreen(
          loading.cover(), nanommo::kLooks[arriving].atmosphere.sky,
          absl::StrCat("The ", nanommo::kLands[arriving].name), world_name);
    } else {
      nanommo::DrawLoadingScreen(loading.cover(),
                                 nanommo::kLooks[land].atmosphere.sky,
                                 world_name, "Entering the world");
    }
    nanommo::UpdateCursor(pointing);
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
    loading.Update(GetFrameTime());

    if (music) {
      switch (jukebox.Update(GetFrameTime(), in_world, music->playing())) {
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
    if (soundboard) {
      soundboard->set_volume(nanommo::Gain(settings.sound.effects));
      switch (passage.Update(loading.covering(), in_world)) {
        case nanommo::Passage::Cue::kDeparture:
          soundboard->Departure();
          break;
        case nanommo::Passage::Cue::kArrival:
          soundboard->Arrival(land);
          break;
        case nanommo::Passage::Cue::kNothing:
          break;
      }
    }
    // The master volume covers each sound the game makes.
    if (audio) {
      SetMasterVolume(
          settings.sound.muted ? 0.0f : nanommo::Gain(settings.sound.master));
    }
  }
  return 0;
}
