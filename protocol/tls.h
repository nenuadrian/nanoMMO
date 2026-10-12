#ifndef NANOMMO_PROTOCOL_TLS_H_
#define NANOMMO_PROTOCOL_TLS_H_

#include <string>

#include "absl/status/statusor.h"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/ssl/stream.hpp"

namespace nanommo {

using TlsStream = asio::ssl::stream<asio::ip::tcp::socket>;

/// TLS 1.3 for a server, with its certificate and its private key, from PEM
/// files.
absl::StatusOr<asio::ssl::context> ServerTls(const std::string& certificate,
                                             const std::string& key);

/// TLS 1.3 for a client, which trusts the certificates our authority signed,
/// and no others.
absl::StatusOr<asio::ssl::context> ClientTls(const std::string& authority);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_TLS_H_
