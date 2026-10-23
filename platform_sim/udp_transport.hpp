#pragma once

#include "microros_bridge.hpp"

namespace sim
{

// UDP link to the micro-ROS Agent (127.0.0.1:8888 unless VCU_AGENT_IP / VCU_AGENT_PORT are set).
const platform::Transport& udp_transport();

} // namespace sim
