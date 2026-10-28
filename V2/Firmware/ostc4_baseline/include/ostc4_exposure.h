/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OSTC4_EXPOSURE_H
#define OSTC4_EXPOSURE_H
#include "ostc4_baseline.h"
#include <stdbool.h>
#include <stddef.h>
#define OSTC4_EXPOSURE_POLICY "ostc4-continuous-air-gf30-70-v1"
/* One serialized synthetic exposure. No sensor, clock, UI or recording access.
 * Explicit initialize starts fresh; rejected actual exposure latches INVALID
 * until another successful initialize. Samples linearly cover <=3600 seconds;
 * callers MUST report any missing exposure with ostc4_exposure_missing().
 * read retains last accepted numbers for diagnostics with INVALID flags.
 * predict covers one hypothetical segment; never advances or invalidates actual
 * exposure, even for rejected predictions. Its output is unavailable on errors.
 * Calls (including baseline APIs) must not overlap. No thread safety is promised.
 */
ostc4_status ostc4_exposure_initialize(const ostc4_initial *initial);
ostc4_status ostc4_exposure_advance(const ostc4_sample *sample);
ostc4_status ostc4_exposure_read(ostc4_observation *observation);
ostc4_status ostc4_exposure_predict(const ostc4_sample *future, ostc4_observation *prediction);
ostc4_status ostc4_exposure_missing(void);
/* Portable checkpoint v1: explicit little-endian integers/IEEE binary32,
 * CRC-32, pinned numerical policy; never a dump of upstream C structs.
 * Storage and clock evidence belong to the caller. See CHECKPOINT.txt.
 * Only complete valid exposure can be saved. Failure leaves buffer untouched.
 */
#define OSTC4_CHECKPOINT_BYTES 293
ostc4_status ostc4_exposure_checkpoint(void *buffer, size_t size);
typedef struct {
    bool time_known;
    uint32_t restart_seconds;
    uint32_t max_age_seconds; /* caller's freshness limit, NOT proof of exposure */
    const ostc4_sample *history; /* complete linear segments AFTER checkpoint */
    size_t count; /* last sample MUST end exactly at restart_seconds */
} ostc4_recovery;
typedef enum {
    OSTC4_RESTORED, OSTC4_CHECKPOINT_CORRUPT, OSTC4_CHECKPOINT_INCOMPATIBLE,
    OSTC4_CHECKPOINT_STALE, OSTC4_RESTART_TIME_UNKNOWN,
    OSTC4_RESTART_EXPOSURE_UNKNOWN, OSTC4_CHECKPOINT_INVALID,
    OSTC4_RESTART_EXPOSURE_REJECTED
} ostc4_restart_status;
/* Always replaces the active scenario. Any failure latches all outputs INVALID,
 * including in a new process. No fresh tissues or surface exposure fallback.
 * Identical restart/checkpoint timestamps require an explicitly known clock and
 * zero history. Replaying the supplied history preserves its sample cadence.
 */
ostc4_restart_status ostc4_exposure_restore(const void *buffer, size_t size,
                                           const ostc4_recovery *recovery);
const char *ostc4_restart_status_name(ostc4_restart_status status);
#endif
