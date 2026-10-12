#include "protocol/tls.h"

#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/ssl/verify_mode.hpp"

namespace nanommo {

namespace {

absl::Status LoadError(const std::string& path, const std::error_code& error) {
  return absl::FailedPreconditionError(
      absl::StrCat("couldn't load ", path, ": ", error.message(),
                   ". tls/certificates.sh makes them."));
}

}  // namespace

absl::StatusOr<asio::ssl::context> ServerTls(const std::string& certificate,
                                             const std::string& key) {
  asio::ssl::context tls(asio::ssl::context::tlsv13_server);
  std::error_code error;
  tls.use_certificate_chain_file(certificate, error);
  if (error) {
    return LoadError(certificate, error);
  }
  tls.use_private_key_file(key, asio::ssl::context::pem, error);
  if (error) {
    return LoadError(key, error);
  }
  return tls;
}

absl::StatusOr<asio::ssl::context> ClientTls(const std::string& authority) {
  asio::ssl::context tls(asio::ssl::context::tlsv13_client);
  std::error_code error;
  tls.load_verify_file(authority, error);
  if (error) {
    return LoadError(authority, error);
  }
  tls.set_verify_mode(asio::ssl::verify_peer);
  return tls;
}

}  // namespace nanommo
