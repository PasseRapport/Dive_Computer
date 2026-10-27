/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Experimental OSTC4 comparison adapter. GPL-3.0-or-later. */
#ifndef OSTC4_BASELINE_H
#define OSTC4_BASELINE_H
#include <stdint.h>
#define OSTC4_TISSUES 16
#define OSTC4_STOPS 50
#define OSTC4_REVISION "7198d203520f3a3cdccf1ccc0bf8af6475ee4819"
#define OSTC4_POLICY "ostc4-beta-7198d20-unchanged-air-gf30-70-v1"

typedef enum {
    OSTC4_UNAVAILABLE, OSTC4_AVAILABLE, OSTC4_LIMITED, OSTC4_INVALID
} ostc4_availability;
typedef enum {
    OSTC4_OK, OSTC4_BAD_INPUT, OSTC4_BAD_TIME, OSTC4_NOT_INITIALIZED,
    OSTC4_NUMERICAL_FAILURE
} ostc4_status;
typedef struct {
    uint32_t timestamp_seconds;
    float surface_pressure_bar;
} ostc4_initial;
typedef struct {
    uint32_t timestamp_seconds;
    float absolute_pressure_bar;
} ostc4_sample;
typedef struct {
    uint32_t timestamp_seconds;
    uint32_t dive_seconds;
    float absolute_pressure_bar;
    float nitrogen_bar[OSTC4_TISSUES];
    float helium_bar[OSTC4_TISSUES];
    float gf_anchor_ambient_bar;
    float gf_low_depth_bar;
    int ndl_seconds;
    int tts_seconds;
    float ceiling_meters;
    unsigned short stop_seconds[OSTC4_STOPS];
    ostc4_availability tissues, ndl, tts, ceiling, stops;
} ostc4_observation;

/* One active, serialized synthetic scenario; no concurrency or recovery API.
 * Initialize explicitly at surface equilibrium (upstream reset convention).
 * Surface 0.8..1.2 bar. Samples: surface..surface+6 bar, integral seconds,
 * strictly increasing, <=3600 s apart. Linear pressure between samples;
 * identical endpoints use the unchanged cached constant-pressure path.
 * Each accepted sample runs the upstream planner once; anchor changes persist.
 * Rejected inputs leave the previous state intact. Read never runs predictions.
 */
ostc4_status ostc4_initialize(const ostc4_initial *initial);
ostc4_status ostc4_advance(const ostc4_sample *sample);
ostc4_status ostc4_read(ostc4_observation *observation);
const char *ostc4_availability_name(ostc4_availability availability);
#endif
