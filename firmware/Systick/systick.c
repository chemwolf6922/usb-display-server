#include <stdint.h>
#include "ch32x035_conf.h"
#include "system_ch32x035.h"

/** Default init values */
static int systick_per_us = (HSI_VALUE / 8) / 1000000;
static int systick_per_ms = (HSI_VALUE / 8) / 1000;

void systick_init()
{
    /** Init Systick */
    SysTick->CNT = 0;
    SysTick->CMP = 0xFFFFFFFFFFFFFFFF;
    SysTick->SR = 0;
    /** Count up, no auto reload, HCLK/8, disable interrupt, turn on counter */
    SysTick->CTLR = 1;

    /** calculate ticks per us / ms */
    systick_per_us = SystemCoreClock / 8 / 1000000;
    systick_per_ms = SystemCoreClock / 8 / 1000;
}

uint64_t systick_get_time_us()
{
    return SysTick->CNT / systick_per_us;
}

void systick_delay_us(uint32_t n)
{
    uint64_t start = SysTick->CNT;
    while (SysTick->CNT - start < n * systick_per_us)
    {
    }
}

void systick_delay_ms(uint32_t n)
{
    uint64_t start = SysTick->CNT;
    while (SysTick->CNT - start < n * systick_per_ms)
    {
    }
}
