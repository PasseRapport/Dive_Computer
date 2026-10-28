/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PC_BENCH_H
#define PC_BENCH_H
#include "ostc4_exposure.h"
#include "exposure_gap.h"
#include <stdbool.h>
#include <stddef.h>
#define PC_BENCH_HISTORY 21601
#define PC_BENCH_PROFILE 1024
struct bench_point { uint32_t seconds; float depth_m; bool gap; };
struct bench_view {
    ostc4_observation result;
    uint32_t seconds, duration, elapsed_seconds;
    struct exposure_gap data_gap;
    float depth_m, rate_m_min, surface_bar, density_kg_m3;
    unsigned speed;
    bool paused, replay, automatic, ended, gap;
    const char *error, *recovery;
    const struct bench_point *history, *profile;
    /* Host-only buffer of exact accepted pressure/time inputs; history_count
     * entries, including fresh surface initialization, never reconstructed depth. */
    const ostc4_sample *exposure;
    size_t history_count, profile_count;
};
/* One PC fixture, serialized on the UI thread. Only fresh() initializes tissues.
 * Replay uses a fixed one-second exposure grid, retaining pressure keyframes.
 * Poll takes elapsed host milliseconds; pause discards them. No wall clock here.
 * Commands take effect at the last completed simulation second.
 */
int pc_bench_fresh(float surface_bar, float density_kg_m3,
                   const ostc4_sample *profile, size_t count);
/* PC-only in-memory checkpoint and explicit restart/fault controls. The known
 * mode supplies every exact accepted input since save, never elapsed time alone.
 * Restarts pause and release replay/auto; Fresh discards the saved checkpoint.
 */
enum pc_restart_mode { PC_RESTART_KNOWN, PC_RESTART_GAP, PC_RESTART_CLOCK,
    PC_RESTART_CORRUPT, PC_RESTART_INCOMPATIBLE, PC_RESTART_STALE };
int pc_bench_checkpoint(void);
ostc4_restart_status pc_bench_restart(enum pc_restart_mode mode);
void pc_bench_poll(uint32_t elapsed_ms);
void pc_bench_pause(bool paused);
/* Report unknown exposure of any duration; invalid until an explicit fresh fixture. */
void pc_bench_mark_missing(void);
/* Explicit synthetic loss beginning at the last accepted sample. The caller's
 * simulation clock continues while unpaused; no exposure samples are invented.
 * Repeated loss commands cannot reset an existing gap or convert unknown time.
 */
void pc_bench_sensor_loss(void);
void pc_bench_speed(unsigned speed);
/* An accepted manual command cancels automatic ascent without resetting exposure. */
void pc_bench_manual(float depth_m, float rate_m_min);
/* Enable/re-engage from the last observed pressure, preserving exposure/history;
 * discard any unobserved manual depth command and fixed replay. Disable holds
 * that observed depth. Both clear pending host time. Return -1 before fresh or
 * after missing exposure, 0 otherwise. Automatic ascent runs at 9 metres/minute
 * toward the deepest currently required stop (engine slots are +0.3 bar each,
 * converted with this fixture's density), or surface when no stop is required.
 * Recompute the command for every exposure second. Hold on unavailable, limited
 * or invalid tissues/ceiling/stops/TTS, or when already above a required stop.
 * Stop time comes from current calculations; no scripted countdown is followed.
 */
int pc_bench_auto(bool enabled);
/* Presentation-only notification after fresh, missing exposure, and each fully
 * committed one-second sample (including graph and ended state). The callback
 * must not change bench/engine state. NULL disconnects; fresh preserves it. */
void pc_bench_set_observer(void (*observer)(const struct bench_view *));
const struct bench_view *pc_bench_read(void);
/* Plot contract: normalized x=time/duration, y=depth/max_depth (downwards). */
void pc_bench_plot(struct bench_point point, uint32_t duration, float max_depth,
                   float *x, float *y);
extern const ostc4_sample pc_bench_two_dives[8];
#endif
