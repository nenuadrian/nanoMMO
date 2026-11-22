#ifndef NANOMMO_CLIENT_LOGIN_SCREEN_H_
#define NANOMMO_CLIENT_LOGIN_SCREEN_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/offscreen.h"
#include "client/renderer.h"
#include "content/figures.pb.h"
#include "content/items.h"
#include "content/rig.pb.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Before the world: the account's login, then the worlds, then the
/// account's characters in the world picked, each drawn as it is, turned with
/// the mouse, to pick one to enter as or delete one, and a screen to make a
/// new one on, with its name and how it looks: its build, skin and hair. All
/// of it over embers rising in the dark, with the game's name over the login.
class LoginScreen {
 public:
  /// The preview's size, in pixels: enough for it to be sharp at its size on
  /// a 4K screen.
  static constexpr int kPreviewWidth = 760;
  static constexpr int kPreviewHeight = 1012;

  /// With the ClientHash of this client's content, to tell the worlds that
  /// have other content apart, its items, to dress a new character in the kit
  /// it'll start with, and its figures and rig, for the looks to choose from.
  LoginScreen(uint64_t content, const ItemTable& items,
              const figures::Figures& figures, const rig::Rig& rig)
      : content_(content), items_(items), figures_(figures), rig_(rig) {}

  /// Draws the screen we're on, and returns what to ask the login server, if
  /// there's anything: a login, a new account, the characters again, a new or
  /// a deleted character, or a token to enter the world as one.
  std::optional<protocol::LoginRequest> Build();

  /// Draws the character picked, for the characters' screen, or the one being
  /// made, as it's turned, before the frame.
  void Render(const Renderer& renderer);

  /// What the login server said to what Build asked: the session, the worlds
  /// and the characters, or an error, such as a name that's taken. A session
  /// that's run out goes back to the login.
  void Show(const protocol::LoginReply& reply);
  void ShowError(std::string error) { error_ = std::move(error); }

  /// The world picked, which a token from an enter request is for.
  const std::optional<protocol::WorldListing>& world() const { return world_; }
  /// The name of the character picked, once it's entering.
  std::string character_name() const;

  /// The character as the preview draws it: its looks, or its id, which they
  /// come from for a character from before there were looks to choose, and
  /// what it wears, or the kit a fresh one starts with.
  static Figure FigureOf(
      const protocol::CharacterListing& character, const ItemTable& items,
      const figures::Figures& figures = figures::Figures::default_instance());

 private:
  enum class Step { kAccount, kWorlds, kCharacters, kCreate };

  std::optional<protocol::LoginRequest> BuildAccount();
  std::optional<protocol::LoginRequest> BuildWorlds();
  std::optional<protocol::LoginRequest> BuildCharacters();
  std::optional<protocol::LoginRequest> BuildCreate();
  /// The character on its stage, in the middle, to turn and look at, with its
  /// name and a line under it.
  void DrawStage(const std::string& name, const std::string& line);
  /// To the screen that makes a character, which starts with looks picked at
  /// random, so that no two players start out alike.
  void StartCreating();
  void PickAnyLooks();
  /// The character being made, as the preview draws it.
  protocol::CharacterListing Draft() const;
  /// A request with the session, for what comes after the login.
  protocol::LoginRequest WithSession() const;
  /// The characters in the world picked, the oldest first.
  std::vector<const protocol::CharacterListing*> Here() const;
  int CountIn(const std::string& world) const;

  uint64_t content_;
  const ItemTable& items_;
  const figures::Figures& figures_;
  const rig::Rig& rig_;
  Step step_ = Step::kAccount;
  std::string session_;
  std::vector<protocol::WorldListing> worlds_;
  std::vector<protocol::CharacterListing> characters_;
  int selected_world_ = 0;
  std::optional<protocol::WorldListing> world_;
  // The character picked, by its id, and how the preview's turned and how
  // near it is.
  uint32_t picked_ = 0;
  float turn_ = 0.0f;
  float distance_ = 3.6f;
  bool deleting_ = false;
  std::array<char, 17> username_{};
  std::array<char, 129> password_{};
  std::array<char, 17> new_name_{};
  // How the character being made looks, so far.
  protocol::Looks looks_;
  std::string error_;
  Offscreen preview_{kPreviewWidth, kPreviewHeight};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LOGIN_SCREEN_H_
