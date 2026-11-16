#include "client/websocket_link.h"

#include <emscripten.h>
#include <emscripten/websocket.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"

namespace nanommo {
namespace {

// How long a socket has to open, and a login to be answered, in
// milliseconds: the game waits, and the browser has the page meanwhile.
constexpr int kOpenWait = 10000;
constexpr int kAnswerWait = 10000;

// How often a wait looks again, in milliseconds.
constexpr int kTick = 10;

}  // namespace

absl::StatusOr<std::unique_ptr<WebSocket>> WebSocket::Open(
    const std::string& url) {
  EmscriptenWebSocketCreateAttributes attributes;
  emscripten_websocket_init_create_attributes(&attributes);
  attributes.url = url.c_str();
  attributes.createOnMainThread = true;
  const EMSCRIPTEN_WEBSOCKET_T handle = emscripten_websocket_new(&attributes);
  if (handle <= 0) {
    return absl::UnavailableError(absl::StrCat("couldn't open ", url));
  }
  std::unique_ptr<WebSocket> socket = absl::WrapUnique(new WebSocket(handle));
  emscripten_websocket_set_onopen_callback(handle, socket.get(), OnOpen);
  emscripten_websocket_set_onmessage_callback(handle, socket.get(), OnMessage);
  emscripten_websocket_set_onerror_callback(handle, socket.get(), OnError);
  emscripten_websocket_set_onclose_callback(handle, socket.get(), OnClose);
  // Asyncify lets the sleep hand the page back to the browser, which opens
  // the socket and calls us back meanwhile.
  for (int waited = 0;
       !socket->opened_ && !socket->closed_ && waited < kOpenWait;
       waited += kTick) {
    emscripten_sleep(kTick);
  }
  if (!socket->open()) {
    return absl::UnavailableError(
        absl::StrCat("couldn't reach the gateway at ", url));
  }
  return socket;
}

WebSocket::~WebSocket() {
  emscripten_websocket_close(socket_, 1000, "");
  emscripten_websocket_delete(socket_);
}

absl::Status WebSocket::Send(std::string_view bytes) {
  if (!open()) {
    return absl::UnavailableError("the gateway closed the connection");
  }
  std::string owned(bytes);
  if (emscripten_websocket_send_binary(socket_, owned.data(),
                                       static_cast<uint32_t>(owned.size())) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    return absl::UnavailableError("couldn't send to the gateway");
  }
  return absl::OkStatus();
}

std::string WebSocket::Take() { return std::exchange(received_, {}); }

void WebSocket::Wait(int milliseconds) {
  for (int waited = 0; received_.empty() && open() && waited < milliseconds;
       waited += kTick) {
    emscripten_sleep(kTick);
  }
}

bool WebSocket::OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* self) {
  static_cast<WebSocket*>(self)->opened_ = true;
  return true;
}

bool WebSocket::OnMessage(int, const EmscriptenWebSocketMessageEvent* event,
                          void* self) {
  static_cast<WebSocket*>(self)->received_.append(
      reinterpret_cast<const char*>(event->data), event->numBytes);
  return true;
}

bool WebSocket::OnError(int, const EmscriptenWebSocketErrorEvent*, void* self) {
  static_cast<WebSocket*>(self)->closed_ = true;
  return true;
}

bool WebSocket::OnClose(int, const EmscriptenWebSocketCloseEvent*, void* self) {
  static_cast<WebSocket*>(self)->closed_ = true;
  return true;
}

absl::StatusOr<protocol::LoginReply> AskOverWebSocket(
    const std::string& url, const protocol::LoginRequest& request) {
  absl::StatusOr<std::unique_ptr<WebSocket>> socket = WebSocket::Open(url);
  if (!socket.ok()) {
    return socket.status();
  }
  if (const absl::Status sent = (*socket)->Send(Frame(request)); !sent.ok()) {
    return sent;
  }
  FrameReader reader;
  for (int waited = 0; waited < kAnswerWait; waited += kTick) {
    reader.Append((*socket)->Take());
    if (const std::optional<std::string> message = reader.Next()) {
      protocol::LoginReply reply;
      if (!reply.ParseFromString(*message)) {
        return absl::DataLossError("couldn't read the login server's reply");
      }
      return reply;
    }
    if (!reader.status().ok()) {
      return reader.status();
    }
    if (!(*socket)->open()) {
      return absl::UnavailableError("the gateway closed before the answer");
    }
    (*socket)->Wait(kTick);
  }
  return absl::DeadlineExceededError("the login server didn't answer");
}

absl::StatusOr<std::unique_ptr<WebSocketLink>> WebSocketLink::Open(
    const std::string& url, const std::string& token) {
  absl::StatusOr<std::unique_ptr<WebSocket>> socket = WebSocket::Open(url);
  if (!socket.ok()) {
    return socket.status();
  }
  protocol::Hello hello;
  hello.set_token(token);
  if (const absl::Status sent = (*socket)->Send(Frame(hello)); !sent.ok()) {
    return sent;
  }
  return absl::WrapUnique(new WebSocketLink(*std::move(socket)));
}

void WebSocketLink::Poll() {
  reader_.Append(socket_->Take());
  while (const std::optional<std::string> message = reader_.Next()) {
    if (!inbox_.Take(*message).ok()) {
      connected_ = false;
      return;
    }
  }
  if (!reader_.status().ok() || !socket_->open()) {
    connected_ = false;
  }
}

void WebSocketLink::Send(const protocol::Input& input) {
  if (connected_ && !socket_->Send(Frame(input)).ok()) {
    connected_ = false;
  }
}

}  // namespace nanommo
