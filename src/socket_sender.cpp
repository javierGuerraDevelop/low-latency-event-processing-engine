#include "socket_sender.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "protocol.h"

namespace {

// How long to wait for the bot to accept a connection or drain a send.
constexpr int socket_timeout_ms = 100;

// Minimum spacing between "bot unreachable" warnings.
constexpr auto warning_interval = std::chrono::seconds { 10 };

// Sends the whole frame, waiting briefly for the socket to drain when needed.
bool send_all(int fd, std::string_view frame)
{
    std::size_t sent = 0;
    while (sent < frame.size()) {
        const ssize_t written = send(fd, frame.data() + sent, frame.size() - sent, MSG_NOSIGNAL);
        if (written > 0) {
            sent += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd writable { fd, POLLOUT, 0 };
            if (poll(&writable, 1, socket_timeout_ms) <= 0) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

} // namespace

std::function<void(const EngineMessage&)> make_socket_sender(const std::string& host, int port)
{
    return [host, port, last_warning = std::chrono::system_clock::time_point { }](
               const EngineMessage& message) mutable {
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return;
        }

        sockaddr_in addr { };
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<std::uint16_t>(port));
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        bool delivered = false;
        fcntl(fd, F_SETFL, O_NONBLOCK);
        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0
            || errno == EINPROGRESS) {
            pollfd writable { fd, POLLOUT, 0 };
            if (poll(&writable, 1, socket_timeout_ms) > 0) {
                int socket_error       = 0;
                socklen_t error_length = sizeof(socket_error);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length);
                if (socket_error == 0) {
                    delivered = send_all(fd, serialize_message(message));
                }
            }
        }
        close(fd);

        if (!delivered) {
            const auto now = std::chrono::system_clock::now();
            if (now - last_warning >= warning_interval) {
                std::cerr << "Discord bot unreachable at " << host << ":" << port
                          << "; dropping messages." << std::endl;
                last_warning = now;
            }
        }
    };
}
