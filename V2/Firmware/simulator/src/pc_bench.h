/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PC_BENCH_H
#define PC_BENCH_H
#include "ostc4_exposure.h"
#include <stdbool.h>
#include <stddef.h>
#define PC_BENCH_HISTORY 21601
#define PC_BENCH_PROFILE 1024
struct bench_point { uint32_t seconds; float depth_m; bool gap; };
struct bench_view {
    ostc4_observation result;
    uint32_t seconds, duration;
    float depth_m, rate_m_min, surface_bar, density_kg_m3;
    unsigned speed;
    bool paused, replay, ended, gap;
    const char *error;
    const struct bench_point *history, *profile;
    size_t history_count, profile_count;
};
/* One PC fixture, serialized on the UI thread. Only fresh() initializes tissues.
 * Replay uses a fixed one-second exposure grid, retaining pressure keyframes.
 * Poll takes elapsed host milliseconds; pause discards them. No wall clock here.
 * Commands take effect at the last completed simulation second.
 */
int pc_bench_fresh(float surface_bar, float density_kg_m3,
                   const ostc4_sample *profile, size_t count);
void pc_bench_poll(uint32_t elapsed_ms);
void pc_bench_pause(bool paused);
/* Report unknown exposure of any duration; invalid until an explicit fresh fixture. */
void pc_bench_mark_missing(void);
void pc_bench_speed(unsigned speed);
void pc_bench_manual(float depth_m, float rate_m_min);
const struct bench_view *pc_bench_read(void);
/* Plot contract: normalized x=time/duration, y=depth/max_depth (downwards). */
void pc_bench_plot(struct bench_point point, uint32_t duration, float max_depth,
                   float *x, float *y);
extern const ostc4_sample pc_bench_two_dives[8];
#endif
