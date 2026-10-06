// Sends shotcall text to the Discord bot over a TCP socket.
// Each invocation opens a new connection, matching the bot's per-connection model.
// Silently no-ops if the bot is not running.

#ifndef SHOTCALLERCPP_SOCKET_SENDER_H
#define SHOTCALLERCPP_SOCKET_SENDER_H

#include <functional>
#include <string>

// Returns a callback that sends callout text over TCP to host:port.
std::function<void(const std::string&, const std::string&)> make_socket_sender(
    const std::string& host = "127.0.0.1", int port = 9999);

#endif // SHOTCALLERCPP_SOCKET_SENDER_H
