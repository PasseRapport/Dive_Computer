#include "sim_dive.h"

void sim_dive_session(struct sim_dive *state, struct watch_data *data,
                      uint64_t now_ms, bool active)
{
    if (active && !state->session_active) {
        *state = (struct sim_dive){.session_active=true,.session_start_ms=now_ms};
        data->elapsed_s=data->runtime_s=data->recovery_s=0;
        data->max_dive_s=data->total_dives=0;
        data->last_depth_dm=data->max_depth_dm=data->current_max_depth_dm=0;
        data->show_recovery=data->recovery_red=data->recovery_ring_visible=false;
        data->recovery_elapsed_ms=data->recovery_target_ms=0;
        data->apnea_active=false;data->apnea_direction=0;
    } else if (!active) {
        state->session_active=false;
        state->diving=false;
        data->apnea_active=false;data->apnea_direction=0;
        data->show_recovery=data->recovery_ring_visible=false;
    }
}

void sim_dive_update(struct sim_dive *state, struct watch_data *data,
                     uint64_t now_ms, uint16_t depth_dm)
{
    data->depth_dm = depth_dm;
    if (state->session_active) data->runtime_s=(now_ms-state->session_start_ms)/1000;
    uint32_t depth_mm=data->depth_mm_valid?data->depth_mm:(uint32_t)depth_dm*100;
    data->apnea_direction=0;
    if (!state->diving && depth_dm > 10) {
        state->diving = true;
        state->start_ms = now_ms;
        state->peak_dm = 0;
        data->total_dives++;
    }
    if (state->diving) {
        data->apnea_direction=depth_mm>state->previous_depth_mm?1:
                              depth_mm<state->previous_depth_mm?-1:0;
        uint32_t duration = now_ms - state->start_ms;
        data->elapsed_s = duration / 1000;
        if (data->elapsed_s > data->max_dive_s) data->max_dive_s = data->elapsed_s;
        if (depth_dm > state->peak_dm) state->peak_dm = depth_dm;
        if (depth_dm > data->max_depth_dm) data->max_depth_dm = depth_dm;
        data->current_max_depth_dm=state->peak_dm;
        data->show_recovery = false;
        data->recovery_ring_visible = false;
        if (depth_dm < 3) {
            state->diving = false;
            data->apnea_direction=0;
            state->has_dived = true;
            state->surface_ms = now_ms;
            state->last_duration_ms = duration ? duration : 1;
            data->last_depth_dm = state->peak_dm;
            if (data->elapsed_s > data->max_dive_s) data->max_dive_s = data->elapsed_s;
        }
    }
    state->previous_depth_mm=depth_mm;
    data->apnea_active=state->diving;
    if (!state->diving && state->has_dived) {
        uint64_t elapsed = now_ms - state->surface_ms;
        data->recovery_target_ms = state->last_duration_ms * 2;
        data->recovery_elapsed_ms = elapsed > data->recovery_target_ms ?
                                   data->recovery_target_ms : elapsed;
        data->recovery_s = elapsed / 1000;
        data->show_recovery = true;
        data->recovery_red = elapsed < data->recovery_target_ms;
        data->recovery_ring_visible = elapsed < (uint64_t)data->recovery_target_ms + 10000;
    }
}
