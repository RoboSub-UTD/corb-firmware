#include "microros_bridge.hpp"

#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include <corb_msgs/msg/thruster_command.h>
#include <corb_msgs/msg/vcu_state.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>
#include <uxr/client/transport.h>

namespace platform
{
namespace
{

BridgeClient* g_client = nullptr;
corb_msgs__msg__ThrusterCommand g_cmd_msg;

const Transport* g_transport = nullptr;

void on_command(const void* msgin)
{
    const auto* m = static_cast<const corb_msgs__msg__ThrusterCommand*>(msgin);
    g_client->on_command(m->seq, m->values, m->count);
}

// Init failures are fatal for this task; a missed publish is dropped (best effort).
bool ok(rcl_ret_t rc, const char* what)
{
    if (rc != RCL_RET_OK)
        std::printf("micro-ROS: %s failed (%d)\n", what, static_cast<int>(rc));
    return rc == RCL_RET_OK;
}

void microros_task(void*)
{
    rmw_uros_set_custom_transport(g_transport->framed, g_transport->args, g_transport->open,
                                  g_transport->close, g_transport->write, g_transport->read);

    // Wait for the agent, then bring everything up.
    while (rmw_uros_ping_agent(100, 1) != RMW_RET_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    std::printf("micro-ROS agent connected\n");

    rcl_allocator_t alloc = rcl_get_default_allocator();
    rclc_support_t support;
    rcl_node_t node;
    rcl_subscription_t sub;
    rcl_publisher_t pub;
    rclc_executor_t exec;

    const bool up =
        ok(rclc_support_init(&support, 0, nullptr, &alloc), "support_init") &&
        ok(rclc_node_init_default(&node, "corb_firmware", "", &support), "node_init") &&
        ok(rclc_subscription_init_default(
               &sub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(corb_msgs, msg, ThrusterCommand),
               "/vcu/thruster_cmd"),
           "subscription_init") &&
        ok(rclc_publisher_init_default(&pub, &node,
                                       ROSIDL_GET_MSG_TYPE_SUPPORT(corb_msgs, msg, VcuState),
                                       "/vcu/state"),
           "publisher_init") &&
        ok(rclc_executor_init(&exec, &support.context, 1, &alloc), "executor_init") &&
        ok(rclc_executor_add_subscription(&exec, &sub, &g_cmd_msg, on_command, ON_NEW_DATA),
           "add_subscription");
    if (!up)
        vTaskDelete(nullptr);

    TickType_t next_pub = xTaskGetTickCount();
    for (;;)
    {
        (void)rclc_executor_spin_some(&exec, RCL_MS_TO_NS(10)); // timeouts are normal
        const TickType_t now = xTaskGetTickCount();
        if (static_cast<int32_t>(now - next_pub) >= 0)
        {
            corb_msgs__msg__VcuState st{};
            g_client->snapshot(st.state, st.command_fresh);
            st.uptime_ms = static_cast<uint32_t>(now);
            if (rcl_publish(&pub, &st, nullptr) != RCL_RET_OK)
            {
                // best effort: drop this sample, the next one follows in 100 ms
            }
            next_pub = now + pdMS_TO_TICKS(100);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

} // namespace

void start_microros(BridgeClient& client, const Transport& transport)
{
    g_client = &client;
    g_transport = &transport;
    corb_msgs__msg__ThrusterCommand__init(&g_cmd_msg);
    xTaskCreate(microros_task, "microros", configMINIMAL_STACK_SIZE * 4, nullptr, 1, nullptr);
}

} // namespace platform
