/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "scuba_model.h"
#include "recorded_dive.h"
#include "ostc4_baseline.h"
/* Presentation only: no engine calls or exposure resets. Depth is physical metres; model.salt_water is the active water selection.
 * Engine reference ceilings/pressure slots convert here. Raw pressure is never
 * reconstructed from display values. Call on every completed simulation step. */
void scuba_calculated_apply(struct scuba_model *model,const ostc4_observation *result,
    uint32_t seconds,float gauge_depth_m,int16_t temperature,uint8_t battery);
/* Controller-authoritative lifecycle for the calculated PC watch. The raw
 * adapter above also supports isolated presentation fixtures without a bench. */
void scuba_calculated_apply_recorded(struct scuba_model *model,const struct recorded_dive *recording,
    const ostc4_observation *result,uint32_t seconds,float gauge_depth_m,
    int16_t temperature,uint8_t battery);
