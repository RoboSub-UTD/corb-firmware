// USB CDC-ACM link to the micro-ROS Agent (appears as /dev/ttyACM0 on the laptop).
#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx.h"
#include "tusb.h"

#include "board.hpp"

namespace board
{
namespace
{

constexpr unsigned kUsbIrqPriority = 6; // numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
constexpr int kWriteTimeoutMs = 50;

// USB needs an exact 48 MHz: take it from the HSI48, trimmed against the host's SOF packets.
void usb_hw_init()
{
    RCC->CR |= RCC_CR_HSI48ON;
    while (!(RCC->CR & RCC_CR_HSI48RDY))
    {
    }
    RCC->APB1HENR |= RCC_APB1HENR_CRSEN;
    CRS->CFGR = (CRS->CFGR & ~CRS_CFGR_SYNCSRC) | (2u << CRS_CFGR_SYNCSRC_Pos); // USB SOF
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
    RCC->D2CCIP2R = (RCC->D2CCIP2R & ~RCC_D2CCIP2R_USBSEL) | (3u << RCC_D2CCIP2R_USBSEL_Pos);

    PWR->CR3 |= PWR_CR3_USB33DEN; // USB regulator detector
    while (!(PWR->CR3 & PWR_CR3_USB33RDY))
    {
    }

    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    for (unsigned pin : {11u, 12u}) // DM, DP
    {
        GPIOA->MODER = (GPIOA->MODER & ~(3u << (pin * 2))) | (2u << (pin * 2));
        GPIOA->OSPEEDR |= 3u << (pin * 2);
        GPIOA->PUPDR &= ~(3u << (pin * 2));
        GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFu << ((pin - 8) * 4))) | (10u << ((pin - 8) * 4));
    }

    RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN;
    RCC->AHB1ENR &= ~RCC_AHB1ENR_USB1OTGHSULPIEN; // embedded FS PHY, not external ULPI
    RCC->AHB1LPENR &= ~RCC_AHB1LPENR_USB1OTGHSULPILPEN;
}

void usb_task(void*)
{
    usb_hw_init();
    // No VBUS sense wired (PA9 is the debug UART): treat the device as always attached.
    tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
    cfg.vbus_sensing = false;
    tud_configure(BOARD_TUD_RHPORT, TUD_CFGID_DWC2, &cfg);
    tusb_rhport_init_t init = {TUSB_ROLE_DEVICE, TUSB_SPEED_FULL};
    tusb_init(BOARD_TUD_RHPORT, &init);
    NVIC_SetPriority(OTG_HS_IRQn, kUsbIrqPriority);
    for (;;)
    {
        tud_task(); // blocks on the USB event queue
    }
}

// ---- micro-ROS transport over CDC ----

bool cdc_open(uxrCustomTransport*) { return true; } // the usb task is started by board::start_usb()
bool cdc_close(uxrCustomTransport*) { return true; }

std::size_t cdc_write(uxrCustomTransport*, const std::uint8_t* buf, std::size_t len, std::uint8_t*)
{
    std::size_t sent = 0;
    TickType_t last_progress = xTaskGetTickCount();
    while (sent < len)
    {
        if (!tud_cdc_connected()) // nobody has the port open: drop, the Agent will resync
            return len;
        const std::uint32_t n = tud_cdc_write(buf + sent, static_cast<std::uint32_t>(len - sent));
        if (n > 0)
        {
            sent += n;
            last_progress = xTaskGetTickCount();
            tud_cdc_write_flush();
        }
        else
        {
            if (static_cast<int>(xTaskGetTickCount() - last_progress) > kWriteTimeoutMs)
                return sent;
            vTaskDelay(1);
        }
    }
    return sent;
}

std::size_t cdc_read(uxrCustomTransport*, std::uint8_t* buf, std::size_t len, int timeout_ms,
                     std::uint8_t*)
{
    const TickType_t start = xTaskGetTickCount();
    for (;;)
    {
        const std::uint32_t n = tud_cdc_read(buf, static_cast<std::uint32_t>(len));
        if (n > 0 || static_cast<int>(xTaskGetTickCount() - start) >= timeout_ms)
            return n;
        vTaskDelay(1);
    }
}

} // namespace

void start_usb() { xTaskCreate(usb_task, "usb", 1024, nullptr, 3, nullptr); }

const platform::Transport& microros_transport()
{
    static const platform::Transport t{true, nullptr, cdc_open, cdc_close, cdc_write, cdc_read};
    return t;
}

} // namespace board

extern "C" void OTG_HS_IRQHandler() { tusb_int_handler(0, true); }

//--------------------------------------------------------------------+
// Descriptors
//--------------------------------------------------------------------+

namespace
{

const tusb_desc_device_t kDevice = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCafe, // TinyUSB test VID: replace if this ever ships
    .idProduct = 0x4011,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1};

enum
{
    ITF_CDC = 0,
    ITF_CDC_DATA,
    ITF_TOTAL
};
constexpr std::uint8_t kEpCdcNotif = 0x81;
constexpr std::uint8_t kEpCdcOut = 0x02;
constexpr std::uint8_t kEpCdcIn = 0x82;

const std::uint8_t kConfig[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 4, kEpCdcNotif, 8, kEpCdcOut, kEpCdcIn, 64)};

const char* const kStrings[] = {
    nullptr, // 0: language, filled in below
    "RoboSub UTD",
    "CORB VCU",
    nullptr, // 3: serial, from the chip's unique ID
    "CORB micro-ROS"};

} // namespace

extern "C" const std::uint8_t* tud_descriptor_device_cb() { return reinterpret_cast<const std::uint8_t*>(&kDevice); }
extern "C" const std::uint8_t* tud_descriptor_configuration_cb(std::uint8_t) { return kConfig; }

extern "C" const std::uint16_t* tud_descriptor_string_cb(std::uint8_t index, std::uint16_t)
{
    static std::uint16_t desc[33];
    char serial[25];
    const char* s;
    std::size_t n;
    if (index == 0)
    {
        desc[1] = 0x0409;
        n = 1;
        desc[0] = static_cast<std::uint16_t>((TUSB_DESC_STRING << 8) | (2 * n + 2));
        return desc;
    }
    if (index == 3)
    {
        const std::uint32_t* uid = reinterpret_cast<const std::uint32_t*>(UID_BASE);
        std::snprintf(serial, sizeof(serial), "%08lX%08lX", static_cast<unsigned long>(uid[0] ^ uid[2]),
                      static_cast<unsigned long>(uid[1]));
        s = serial;
    }
    else if (index < sizeof(kStrings) / sizeof(kStrings[0]) && kStrings[index] != nullptr)
    {
        s = kStrings[index];
    }
    else
    {
        return nullptr;
    }
    n = std::strlen(s);
    if (n > 31)
        n = 31;
    for (std::size_t i = 0; i < n; ++i)
        desc[1 + i] = static_cast<std::uint8_t>(s[i]);
    desc[0] = static_cast<std::uint16_t>((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return desc;
}
