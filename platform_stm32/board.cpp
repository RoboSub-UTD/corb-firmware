#include "board.hpp"

#include <algorithm>
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"


namespace board
{
namespace
{

constexpr std::uint32_t kConsoleBaud = 115200;
constexpr std::uint32_t kPwmNeutralUs = 1500;
constexpr float kPwmSpanUs = 400.0f; // +/- around neutral
constexpr std::size_t kPwmChannels = 4;

[[noreturn]] void clock_fail()
{
    // Solid-on LED means the clock tree did not come up (wrong HSE value or no crystal).
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;
    GPIOG->MODER = (GPIOG->MODER & ~(3u << 14)) | (1u << 14);
    GPIOG->BSRR = 1u << 7;
    for (;;)
    {
    }
}

void init_clocks()
{
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 3;   // 15 MHz / 3 = 5 MHz
    osc.PLL.PLLN = 160; // VCO 800 MHz
    osc.PLL.PLLP = 2;   // 400 MHz sysclk
    osc.PLL.PLLQ = 4;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_2; // 4..8 MHz input
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        clock_fail();

    RCC_ClkInitTypeDef clk{};
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1; // 400 MHz core
    clk.AHBCLKDivider = RCC_HCLK_DIV2;   // 200 MHz AHB / AXI
    clk.APB3CLKDivider = RCC_APB3_DIV2;  // 100 MHz
    clk.APB1CLKDivider = RCC_APB1_DIV2;  // 100 MHz (timers 200 MHz)
    clk.APB2CLKDivider = RCC_APB2_DIV2;  // 100 MHz (timers 200 MHz)
    clk.APB4CLKDivider = RCC_APB4_DIV2;  // 100 MHz
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        clock_fail();
}

void gpio_af(GPIO_TypeDef* port, unsigned pin, unsigned af, bool pullup)
{
    port->MODER = (port->MODER & ~(3u << (pin * 2))) | (2u << (pin * 2));
    port->OSPEEDR |= 3u << (pin * 2);
    port->PUPDR = (port->PUPDR & ~(3u << (pin * 2))) | ((pullup ? 1u : 0u) << (pin * 2));
    const unsigned reg = pin / 8;
    const unsigned shift = (pin % 8) * 4;
    port->AFR[reg] = (port->AFR[reg] & ~(0xFu << shift)) | (af << shift);
}

void gpio_out(GPIO_TypeDef* port, unsigned pin, bool high)
{
    // Set the level first so the pin never glitches to the wrong state.
    port->BSRR = high ? (1u << pin) : (1u << (pin + 16));
    port->MODER = (port->MODER & ~(3u << (pin * 2))) | (1u << (pin * 2));
}

// ---- UART (register level, interrupt driven RX ring, polled TX) ----

class Uart
{
public:
    explicit Uart(USART_TypeDef* u) : u_(u) {}

    void init(std::uint32_t pclk_hz, std::uint32_t baud, bool rx_irq)
    {
        u_->CR1 = 0;
        u_->BRR = (pclk_hz + baud / 2) / baud;
        u_->CR1 = USART_CR1_TE | USART_CR1_RE | (rx_irq ? USART_CR1_RXNEIE_RXFNEIE : 0u);
        u_->CR1 |= USART_CR1_UE;
    }

    void put(std::uint8_t c)
    {
        while (!(u_->ISR & USART_ISR_TXE_TXFNF))
        {
        }
        u_->TDR = c;
    }

    void flush()
    {
        while (!(u_->ISR & USART_ISR_TC))
        {
        }
    }

    void on_irq()
    {
        if (u_->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
            u_->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
        while (u_->ISR & USART_ISR_RXNE_RXFNE)
        {
            const std::uint8_t c = static_cast<std::uint8_t>(u_->RDR);
            const std::uint16_t next = static_cast<std::uint16_t>((head_ + 1u) % kRing);
            if (next != tail_)
            {
                ring_[head_] = c;
                head_ = next;
            }
        }
    }

    // Single consumer. Returns false if empty.
    bool get(std::uint8_t& c)
    {
        if (tail_ == head_)
            return false;
        c = ring_[tail_];
        tail_ = static_cast<std::uint16_t>((tail_ + 1u) % kRing);
        return true;
    }

private:
    static constexpr std::uint16_t kRing = 2048;
    USART_TypeDef* u_;
    volatile std::uint8_t ring_[kRing]{};
    volatile std::uint16_t head_ = 0;
    volatile std::uint16_t tail_ = 0;
};

Uart g_console(USART1);

// ---- PWM ----

void init_pwm()
{
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    for (unsigned pin : {9u, 11u, 13u, 14u})
        gpio_af(GPIOE, pin, 1, false);

    TIM1->PSC = 200 - 1;   // 200 MHz timer clock -> 1 MHz
    TIM1->ARR = 20000 - 1; // 50 Hz
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = TIM1->CCR4 = kPwmNeutralUs;
    TIM1->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE | (6u << TIM_CCMR1_OC2M_Pos) |
                  TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE | (6u << TIM_CCMR2_OC4M_Pos) |
                  TIM_CCMR2_OC4PE;
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

} // namespace

void init()
{
    // Kill line first: SAFE (low) before anything else can run.
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    gpio_out(GPIOB, 0, false);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // DWT cycle counter backs HAL_GetTick
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    init_clocks();

    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOGEN;
    gpio_out(GPIOG, 7, false);

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    gpio_af(GPIOA, 9, 7, false);
    gpio_af(GPIOA, 10, 7, true);
    g_console.init(100000000u, kConsoleBaud, false);

    init_pwm();
}

void led_set(bool on) { GPIOG->BSRR = on ? (1u << 7) : (1u << (7 + 16)); }

void Thrusters::write(const corb::ThrusterArray& values, std::size_t count)
{
    volatile std::uint32_t* const ccr[kPwmChannels] = {&TIM1->CCR1, &TIM1->CCR2, &TIM1->CCR3,
                                                        &TIM1->CCR4};
    for (std::size_t i = 0; i < kPwmChannels; ++i)
    {
        const float v = i < count ? std::clamp(values[i], -1.0f, 1.0f) : 0.0f;
        *ccr[i] = static_cast<std::uint32_t>(static_cast<float>(kPwmNeutralUs) + v * kPwmSpanUs);
    }
    first_ = values[0];
}

void KillLine::set_run(bool run)
{
    GPIOB->BSRR = run ? (1u << 0) : (1u << (0 + 16));
    std::printf("kill line -> %s\n", run ? "RUN" : "SAFE");
}

} // namespace board

// Console bytes for printf (newlib _write).
extern "C" void board_console_put(char c)
{
    if (c == '\n')
        board::g_console.put('\r');
    board::g_console.put(static_cast<std::uint8_t>(c));
}
