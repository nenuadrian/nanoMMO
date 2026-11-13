#ifndef NANOMMO_PROTOCOL_TLS_H_
#define NANOMMO_PROTOCOL_TLS_H_

#include <string>
#include <system_error>

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

/// Has a client's handshake check that the server's certificate names host,
/// a domain name or an address, in a way HandshakeError can tell apart.
void ExpectName(TlsStream& stream, const std::string& host);

/// Why a client's handshake with host failed, and what to do about it.
std::string HandshakeError(TlsStream& stream, const std::string& host,
                           const std::error_code& error);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_TLS_H_
