#ifndef NANOMMO_CLIENT_LOGIN_SCREEN_H_
#define NANOMMO_CLIENT_LOGIN_SCREEN_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

class LoginScreen {
 public:
  /// With the ClientHash of this client's content, to tell the worlds that
  /// have other content apart.
  explicit LoginScreen(uint64_t content) : content_(content) {}

  /// Draws the worlds, or the login for the world picked, and returns what
  /// to ask the login server, if there's anything: the worlds when they're
  /// shown and refreshed, or a login.
  std::optional<protocol::LoginRequest> Build();

  void ShowWorlds(std::vector<protocol::WorldListing> worlds);
  void ShowError(std::string error) { error_ = std::move(error); }

  const std::optional<protocol::WorldListing>& world() const { return world_; }

 private:
  std::optional<protocol::LoginRequest> BuildWorlds();
  std::optional<protocol::LoginRequest> BuildLogin();

  uint64_t content_;
  std::vector<protocol::WorldListing> worlds_;
  int selected_ = 0;
  std::optional<protocol::WorldListing> world_;
  std::array<char, 17> username_{};
  std::array<char, 129> password_{};
  std::string error_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LOGIN_SCREEN_H_
