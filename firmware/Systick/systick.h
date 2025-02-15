#pragma once

#include <stdint.h>

void systick_init();

uint64_t systick_get_time_us();

void systick_delay_us(uint32_t n);

void systick_delay_ms(uint32_t n);
