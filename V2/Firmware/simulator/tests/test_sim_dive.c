#include "sim_dive.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct sim_dive state = {0};
    struct watch_data data = {0};
    sim_dive_update(&state, &data, 0, 0);
    assert(!data.show_recovery && !data.recovery_ring_visible);
    sim_dive_update(&state, &data, 1000, 10);
    assert(!state.diving);
    sim_dive_update(&state, &data, 2000, 20);
    assert(state.diving && data.total_dives == 1);
    sim_dive_update(&state, &data, 7000, 120);
    assert(data.elapsed_s == 5 && !data.recovery_ring_visible);
    sim_dive_update(&state, &data, 12000, 0);
    assert(!state.diving && data.recovery_red && data.recovery_ring_visible);
    assert(data.last_depth_dm == 120 && data.max_depth_dm == 120);
    assert(data.elapsed_s == 10 && data.recovery_target_ms == 20000);
    sim_dive_update(&state, &data, 22000, 0);
    assert(data.recovery_elapsed_ms == 10000 && data.recovery_red);
    sim_dive_update(&state, &data, 32000, 0);
    assert(!data.recovery_red && data.recovery_ring_visible);
    assert(data.recovery_elapsed_ms == data.recovery_target_ms);
    sim_dive_update(&state, &data, 41999, 0);
    assert(data.recovery_ring_visible);
    sim_dive_update(&state, &data, 42000, 0);
    assert(!data.recovery_ring_visible && data.show_recovery);
    sim_dive_update(&state, &data, 45000, 50);
    assert(!data.show_recovery && !data.recovery_ring_visible && data.total_dives == 2);
    sim_dive_update(&state, &data, 47000, 0);
    assert(data.recovery_target_ms == 4000 && data.last_depth_dm == 50);
    assert(data.max_depth_dm == 120 && data.max_dive_s == 10);
    sim_dive_update(&state, &data, 51000, 0);
    assert(!data.recovery_red && data.recovery_ring_visible);
    sim_dive_update(&state, &data, 52000, 40);
    assert(!data.recovery_ring_visible);
    puts("DIVE MODEL CHECK PASS: V1 thresholds, per-dive target, red/green, 10-second hold, restart, stats");
}
