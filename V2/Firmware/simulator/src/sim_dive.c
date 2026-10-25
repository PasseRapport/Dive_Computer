#include "sim_dive.h"

void sim_dive_update(struct sim_dive *state, struct watch_data *data,
                     uint64_t now_ms, uint16_t depth_dm)
{
    data->depth_dm = depth_dm;
    if (!state->diving && depth_dm > 10) {
        state->diving = true;
        state->start_ms = now_ms;
        state->peak_dm = 0;
        data->total_dives++;
    }
    if (state->diving) {
        uint32_t duration = now_ms - state->start_ms;
        data->elapsed_s = duration / 1000;
        if (depth_dm > state->peak_dm) state->peak_dm = depth_dm;
        if (depth_dm > data->max_depth_dm) data->max_depth_dm = depth_dm;
        data->show_recovery = false;
        data->recovery_ring_visible = false;
        if (depth_dm < 3) {
            state->diving = false;
            state->has_dived = true;
            state->surface_ms = now_ms;
            state->last_duration_ms = duration ? duration : 1;
            data->last_depth_dm = state->peak_dm;
            if (data->elapsed_s > data->max_dive_s) data->max_dive_s = data->elapsed_s;
        }
    }
    if (!state->diving && state->has_dived) {
        uint64_t elapsed = now_ms - state->surface_ms;
        data->recovery_target_ms = state->last_duration_ms * 2;
        data->recovery_elapsed_ms = elapsed > data->recovery_target_ms ?
                                   data->recovery_target_ms : elapsed;
        data->recovery_s = elapsed / 1000;
        data->show_recovery = elapsed < 600000;
        data->recovery_red = elapsed < data->recovery_target_ms;
        data->recovery_ring_visible = elapsed < (uint64_t)data->recovery_target_ms + 10000;
    }
}
