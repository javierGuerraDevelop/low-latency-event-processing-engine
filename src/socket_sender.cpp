#include "socket_sender.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>

std::function<void(const std::string&, const std::string&)> make_socket_sender(
    const std::string& host, int port)
{
    return [host, port](const std::string& /*enemy_id*/, const std::string& callout) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return;

        sockaddr_in addr { };
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<std::uint16_t>(port));
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            send(fd, callout.data(), callout.size(), 0);
        }

        close(fd);
    };
}
