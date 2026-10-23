#pragma once

#include <cstddef>
#include <cstdint>

struct uxrCustomTransport;

namespace platform
{

// What the micro-ROS task needs from the application. Called on the bridge's
// task; implementations do their own locking.
class BridgeClient
{
public:
    virtual ~BridgeClient() = default;
    virtual bool on_command(std::uint32_t seq, const float* values, std::size_t n) = 0;
    virtual void snapshot(std::uint8_t& state, bool& command_fresh) = 0;

protected:
    BridgeClient() = default;
    BridgeClient(const BridgeClient&) = default;
    BridgeClient& operator=(const BridgeClient&) = default;
};

// Byte transport to the micro-ROS Agent (UDP in the sim, a UART on the chip).
// The callbacks have the signatures rmw_uros_set_custom_transport() expects;
// `args` is passed back to them as uxrCustomTransport::args.
struct Transport
{
    bool framed; // true if the link needs XRCE framing (serial), false for datagrams (UDP)
    void* args;
    bool (*open)(uxrCustomTransport*);
    bool (*close)(uxrCustomTransport*);
    std::size_t (*write)(uxrCustomTransport*, const std::uint8_t*, std::size_t, std::uint8_t*);
    std::size_t (*read)(uxrCustomTransport*, std::uint8_t*, std::size_t, int, std::uint8_t*);
};

// Spawns a FreeRTOS task that connects to the micro-ROS Agent over `transport`,
// subscribes to /vcu/thruster_cmd and publishes /vcu/state. `client` and
// `transport` must outlive the task.
void start_microros(BridgeClient& client, const Transport& transport);

} // namespace platform
