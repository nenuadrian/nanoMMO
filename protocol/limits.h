#ifndef NANOMMO_PROTOCOL_LIMITS_H_
#define NANOMMO_PROTOCOL_LIMITS_H_

namespace nanommo {

/// How many characters an account can have in each world. The login server
/// turns one more away, and the client doesn't offer to make it.
inline constexpr int kCharactersPerWorld = 4;

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_LIMITS_H_
