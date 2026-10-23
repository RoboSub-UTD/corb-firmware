#include "udp_transport.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>

#include <uxr/client/transport.h>

namespace sim
{
namespace
{

struct UdpTransport
{
    int fd = -1;
    sockaddr_in agent{};
};
UdpTransport g_udp;

bool udp_open(uxrCustomTransport* t)
{
    auto* u = static_cast<UdpTransport*>(t->args);
    const char* ip = std::getenv("VCU_AGENT_IP");
    const char* port = std::getenv("VCU_AGENT_PORT");
    u->agent = {};
    u->agent.sin_family = AF_INET;
    u->agent.sin_port = htons(static_cast<std::uint16_t>(port ? std::atoi(port) : 8888));
    if (inet_pton(AF_INET, ip ? ip : "127.0.0.1", &u->agent.sin_addr) != 1)
        return false;
    u->fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    return u->fd >= 0;
}

bool udp_close(uxrCustomTransport* t)
{
    auto* u = static_cast<UdpTransport*>(t->args);
    if (u->fd >= 0)
        ::close(u->fd);
    u->fd = -1;
    return true;
}

size_t udp_write(uxrCustomTransport* t, const uint8_t* buf, size_t len, uint8_t* err)
{
    auto* u = static_cast<UdpTransport*>(t->args);
    const ssize_t n = ::sendto(u->fd, buf, len, 0, reinterpret_cast<const sockaddr*>(&u->agent),
                               sizeof(u->agent));
    if (n < 0)
    {
        *err = 1;
        return 0;
    }
    return static_cast<size_t>(n);
}

size_t udp_read(uxrCustomTransport* t, uint8_t* buf, size_t len, int timeout, uint8_t* err)
{
    auto* u = static_cast<UdpTransport*>(t->args);
    pollfd pfd{u->fd, POLLIN, 0};
    if (::poll(&pfd, 1, timeout) <= 0)
    {
        *err = 0;
        return 0;
    }
    const ssize_t n = ::recv(u->fd, buf, len, 0);
    if (n < 0)
    {
        *err = 1;
        return 0;
    }
    return static_cast<size_t>(n);
}

} // namespace

const platform::Transport& udp_transport()
{
    static const platform::Transport t{false, &g_udp, udp_open, udp_close, udp_write, udp_read};
    return t;
}

} // namespace sim
