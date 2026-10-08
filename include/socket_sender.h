// Sends engine messages to the Discord bot over a newline-framed TCP socket.
// Each invocation opens a short-lived connection; connection and send timeouts
// keep the scheduler from blocking when the bot is unavailable.

#ifndef SHOTCALLERCPP_SOCKET_SENDER_H
#define SHOTCALLERCPP_SOCKET_SENDER_H

#include <functional>
#include <string>

#include "engine.h"

// Returns a callback that sends one message per connection to host:port.
std::function<void(const EngineMessage&)> make_socket_sender(
    const std::string& host = "127.0.0.1", int port = 9999);

#endif // SHOTCALLERCPP_SOCKET_SENDER_H
