#pragma once

int cpu_cycle_counter_open();

int cpu_cycle_counter_reset(int fd);
long long cpu_cycle_counter_get_result(int fd);
