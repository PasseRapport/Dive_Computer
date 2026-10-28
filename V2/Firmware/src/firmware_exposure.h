/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FIRMWARE_EXPOSURE_H
#define FIRMWARE_EXPOSURE_H
#include "ostc4_exposure.h"
/* Development adapter: serialized calls from the application thread only.
 * No sensor/restart recovery is implemented. Only an explicit synthetic start
 * is supported. Input is absolute Pa and elapsed milliseconds on one clock.
 * Whole seconds must be supplied; missing/invalid data latches INVALID.
 */
typedef struct {
    bool synthetic;
    ostc4_observation calculation;
} firmware_exposure_result;
ostc4_status firmware_exposure_start_synthetic(uint64_t timestamp_ms, uint32_t surface_pa);
ostc4_status firmware_exposure_sample(uint64_t timestamp_ms, uint32_t absolute_pa, bool valid);
ostc4_status firmware_exposure_read(firmware_exposure_result *result);
/* Compiled on target, executed on host: independent fresh-tissue literal,
 * pressure/time conversion, prediction isolation and missing-data invalidation.
 * Leaves the synthetic exposure INVALID. Zero means all checks passed.
 */
int firmware_exposure_smoke(void);
#endif
