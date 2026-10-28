/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "scuba_model.h"
#include "ostc4_baseline.h"
/* Presentation only: no engine calls or exposure resets. Depth uses the same
 * 10 m/bar convention as raw engine ceilings/stops. Raw pressure is never
 * reconstructed from display values. Call on every completed simulation step. */
void scuba_calculated_apply(struct scuba_model *model,const ostc4_observation *result,
    uint32_t seconds,float gauge_depth_m,int16_t temperature,uint8_t battery);
