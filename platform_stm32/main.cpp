#include <cstdio>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"

#include "board.hpp"
#include "vcu_app.hpp"

#ifdef VCU_ENABLE_MICROROS
#include <rcutils/allocator.h>

namespace
{

// Route micro-ROS allocations through the FreeRTOS heap. Each block carries an 8-byte header
// holding its size so realloc can copy (heap_4 has no realloc of its own).
constexpr std::size_t kHdr = 8;

void* mr_allocate(std::size_t size, void*)
{
    auto* p = static_cast<unsigned char*>(pvPortMalloc(size + kHdr));
    if (p == nullptr)
        return nullptr;
    *reinterpret_cast<std::size_t*>(p) = size;
    return p + kHdr;
}

void mr_deallocate(void* ptr, void*)
{
    if (ptr != nullptr)
        vPortFree(static_cast<unsigned char*>(ptr) - kHdr);
}

void* mr_reallocate(void* ptr, std::size_t size, void* state)
{
    if (ptr == nullptr)
        return mr_allocate(size, state);
    if (size == 0)
    {
        mr_deallocate(ptr, state);
        return nullptr;
    }
    const std::size_t old = *reinterpret_cast<std::size_t*>(static_cast<unsigned char*>(ptr) - kHdr);
    void* fresh = mr_allocate(size, state);
    if (fresh != nullptr)
    {
        __builtin_memcpy(fresh, ptr, old < size ? old : size);
        mr_deallocate(ptr, state);
    }
    return fresh;
}

void* mr_zero_allocate(std::size_t n, std::size_t size, void* state)
{
    void* p = mr_allocate(n * size, state);
    if (p != nullptr)
        __builtin_memset(p, 0, n * size);
    return p;
}

} // namespace
#endif

namespace
{

// Heartbeat so a quick look at the board shows the scheduler is alive.
void blink_task(void*)
{
    for (;;)
    {
        board::led_set(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        board::led_set(false);
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

} // namespace

extern "C" void vAssertCalled()
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

extern "C" void vApplicationMallocFailedHook() { vAssertCalled(); }

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char*) { vAssertCalled(); }

int main()
{
    board::init();
    std::setvbuf(stdout, nullptr, _IONBF, 0); // no lazily malloc'd stdio buffer shared by tasks
    std::printf("\ncorb-firmware on STM32H723 @ %u MHz\n",
                static_cast<unsigned>(SystemCoreClock / 1000000u));

    static board::Thrusters thrusters;
    static board::KillLine kill_line;
    static platform::VcuApp app(thrusters, kill_line, {false});
    app.set_log_probe(thrusters.first());

#ifdef VCU_ENABLE_MICROROS
    rcutils_allocator_t alloc = rcutils_get_zero_initialized_allocator();
    alloc.allocate = mr_allocate;
    alloc.deallocate = mr_deallocate;
    alloc.reallocate = mr_reallocate;
    alloc.zero_allocate = mr_zero_allocate;
    alloc.state = nullptr;
    if (!rcutils_set_default_allocator(&alloc))
        std::printf("rcutils_set_default_allocator failed\n");
    platform::start_microros(app, board::microros_transport());
#else
    std::printf("built without micro-ROS: no commands will arrive\n");
#endif

    board::start_usb();
    xTaskCreate(blink_task, "blink", 256, nullptr, 1, nullptr);
    app.start_control_task();
    vTaskStartScheduler();
    for (;;)
    {
    }
}
