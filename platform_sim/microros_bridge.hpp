#pragma once

#include <cstddef>
#include <cstdint>

namespace sim
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

// Spawns a FreeRTOS task that connects to the micro-ROS Agent over UDP
// (127.0.0.1:8888 unless VCU_AGENT_IP / VCU_AGENT_PORT are set), subscribes to
// /vcu/thruster_cmd and publishes /vcu/state. `client` must outlive the task.
void start_microros(BridgeClient& client);

} // namespace sim
