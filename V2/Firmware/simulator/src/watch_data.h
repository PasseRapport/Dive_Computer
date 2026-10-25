#pragma once
#include <stdint.h>
#include <stdbool.h>

/* UI data only: no sensor or Zephyr dependency. Depth is in decimetres. */
struct watch_data {
    uint16_t depth_dm;
    uint16_t heading_degrees;
    uint32_t elapsed_s;
    int16_t temperature_c;
    uint8_t battery_percent;
    uint16_t last_depth_dm, max_depth_dm;
    uint32_t max_dive_s, total_dives, runtime_s, recovery_s;
    bool show_recovery, recovery_red, recovery_ring_visible;
    uint32_t recovery_elapsed_ms, recovery_target_ms;
    uint8_t clock_hour, clock_minute;
};

