#ifndef NANOMMO_CLIENT_LOGIN_SCREEN_H_
#define NANOMMO_CLIENT_LOGIN_SCREEN_H_

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "protocol/nanommo.pb.h"

namespace nanommo {

class LoginScreen {
 public:
  std::optional<protocol::LoginRequest> Build();

  void ShowError(std::string error) { error_ = std::move(error); }

 private:
  std::array<char, 17> username_{};
  std::array<char, 129> password_{};
  std::string error_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_LOGIN_SCREEN_H_
