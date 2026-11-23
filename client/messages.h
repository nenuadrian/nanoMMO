#ifndef NANOMMO_CLIENT_MESSAGES_H_
#define NANOMMO_CLIENT_MESSAGES_H_

#include <string>

#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A server's sentence in the language in use: its English translated, and
/// what goes in it filled in, the content's names in the language too, and
/// the names that players gave as they are. In English, it's the English the
/// server has for it.
std::string Translated(const protocol::Message& message,
                       const Content& content);

/// One from the login server, which names nothing of the content's.
std::string Translated(const protocol::Message& message);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_MESSAGES_H_
