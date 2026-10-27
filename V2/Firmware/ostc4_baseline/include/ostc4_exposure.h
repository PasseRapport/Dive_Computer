/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OSTC4_EXPOSURE_H
#define OSTC4_EXPOSURE_H
#include "ostc4_baseline.h"
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
#endif
