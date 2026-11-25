#include "protocol/tls.h"

#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/ip/address.hpp"
#include "asio/ssl/verify_mode.hpp"

namespace nanommo {

namespace {

absl::Status LoadError(const std::string& path, const std::error_code& error) {
  return absl::FailedPreconditionError(
      absl::StrCat("couldn't load ", path, ": ", error.message(),
                   ". scripts/certificates.sh makes them."));
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

void ExpectName(TlsStream& stream, const std::string& host) {
  // OpenSSL checks the name itself, and keeps why it failed for us to read.
  X509_VERIFY_PARAM* check = SSL_get0_param(stream.native_handle());
  std::error_code not_an_address;
  asio::ip::make_address(host, not_an_address);
  if (not_an_address) {
    X509_VERIFY_PARAM_set1_host(check, host.c_str(), host.size());
  } else {
    X509_VERIFY_PARAM_set1_ip_asc(check, host.c_str());
  }
}

std::string HandshakeError(TlsStream& stream, const std::string& host,
                           const std::error_code& error) {
  switch (SSL_get_verify_result(stream.native_handle())) {
    case X509_V_ERR_HOSTNAME_MISMATCH:
    case X509_V_ERR_IP_ADDRESS_MISMATCH:
      return absl::StrCat("its certificate doesn't name ", host,
                          ". On the server, make it again with "
                          "scripts/certificates.sh tls ",
                          host, ", and start the servers again");
    // Every authority scripts/certificates.sh makes has the same name, so
    // another one's certificate fails on its signature.
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      return "its certificate isn't from the authority we trust. Copy the "
             "server's tls/ca.crt here, or set NANOMMO_TLS_CA to where it is";
    case X509_V_ERR_CERT_HAS_EXPIRED:
      return "its certificate has expired. On the server, make new ones with "
             "scripts/certificates.sh";
    default:
      return error.message();
  }
}

}  // namespace nanommo
