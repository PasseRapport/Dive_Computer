#pragma once
#include "watch_data.h"

/* Desktop dive simulation. Mirrors V1 thresholds and its 2 x duration rule. */
struct sim_dive {
    bool diving, has_dived;
    uint64_t start_ms, surface_ms;
    uint32_t last_duration_ms;
    uint16_t peak_dm;
};
void sim_dive_update(struct sim_dive *state, struct watch_data *data,
                     uint64_t now_ms, uint16_t depth_dm);
