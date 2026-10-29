/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORDED_DIVE_H
#define RECORDED_DIVE_H
#include <stdbool.h>
#include <stdint.h>
/* Controller-owned recording state, independent of continuous exposure.
 * Seconds use accepted simulation samples, never the host clock. */
struct recorded_dive {
    bool armed, active;
    uint64_t session_id; /* PC synthetic exposure identity; independent of exposure clock. */
    uint16_t start_depth_dm;
    uint16_t max_depth_dm; /* Original physical peak, never reprojected from later preferences. */
    uint16_t density_kg_m3; /* Fixed at recording start; retained after completion. */
    uint32_t started_seconds, completed_seconds, dive_seconds, surface_seconds;
    uint32_t dives_started, dives_completed;
};
#endif
