#ifndef NANOMMO_CLIENT_WEBSOCKET_LINK_H_
#define NANOMMO_CLIENT_WEBSOCKET_LINK_H_

#include <emscripten/websocket.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "client/inbox.h"
#include "client/link.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A browser's WebSocket, to the gateway: the bytes it's sent, as they come,
/// for whoever reads them. In a browser.
class WebSocket {
 public:
  /// Opens the url, and waits for it to open, letting the page carry on
  /// meanwhile, or returns why it didn't.
  static absl::StatusOr<std::unique_ptr<WebSocket>> Open(
      const std::string& url);

  WebSocket(const WebSocket&) = delete;
  WebSocket& operator=(const WebSocket&) = delete;
  ~WebSocket();

  absl::Status Send(std::string_view bytes);

  /// What's come since the last time.
  std::string Take();

  bool open() const { return opened_ && !closed_; }

  /// Waits for something to come, or the socket to close, letting the page
  /// carry on meanwhile, for up to the milliseconds.
  void Wait(int milliseconds);

 private:
  explicit WebSocket(EMSCRIPTEN_WEBSOCKET_T socket) : socket_(socket) {}

  static bool OnOpen(int type, const EmscriptenWebSocketOpenEvent* event,
                     void* self);
  static bool OnMessage(int type, const EmscriptenWebSocketMessageEvent* event,
                        void* self);
  static bool OnError(int type, const EmscriptenWebSocketErrorEvent* event,
                      void* self);
  static bool OnClose(int type, const EmscriptenWebSocketCloseEvent* event,
                      void* self);

  EMSCRIPTEN_WEBSOCKET_T socket_;
  bool opened_ = false;
  bool closed_ = false;
  std::string received_;
};

/// The login server's answer to the request, through the gateway at url.
absl::StatusOr<protocol::LoginReply> AskOverWebSocket(
    const std::string& url, const protocol::LoginRequest& request);

/// A world server, through the gateway, over a browser's WebSocket: the same
/// messages as over TLS on a computer, with the world over the same stream.
class WebSocketLink final : public Link {
 public:
  /// Opens the world's url at the gateway, and says hello with the token.
  static absl::StatusOr<std::unique_ptr<WebSocketLink>> Open(
      const std::string& url, const std::string& token);

  void Poll() override;
  void Send(const protocol::Input& input) override;
  bool connected() const override { return connected_; }
  const protocol::World& world() const override { return inbox_.world(); }
  const std::deque<protocol::Chat>& chat() const override {
    return inbox_.chat();
  }
  int64_t whole() const override { return inbox_.whole(); }
  const std::optional<protocol::Move>& move() const override {
    return inbox_.move();
  }
  const protocol::Inventory& inventory() const override {
    return inbox_.inventory();
  }
  const protocol::Quests& quests() const override { return inbox_.quests(); }
  const protocol::Guild& guild() const override { return inbox_.guild(); }
  const protocol::Trade& trade() const override { return inbox_.trade(); }
  const protocol::Duel& duel() const override { return inbox_.duel(); }
  std::vector<protocol::Event> TakeEvents() override {
    return inbox_.TakeEvents();
  }
  std::optional<protocol::GuildInvite> TakeInvite() override {
    return inbox_.TakeInvite();
  }

 private:
  explicit WebSocketLink(std::unique_ptr<WebSocket> socket)
      : socket_(std::move(socket)) {}

  std::unique_ptr<WebSocket> socket_;
  FrameReader reader_;
  Inbox inbox_;
  bool connected_ = true;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_WEBSOCKET_LINK_H_
