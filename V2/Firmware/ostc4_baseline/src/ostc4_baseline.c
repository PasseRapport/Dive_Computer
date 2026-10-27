/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Experimental comparison adapter; upstream calculations live in upstream/. */
#include "ostc4_baseline.h"
#include "decom.h"
#include "buehlmann.h"
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

/* Upstream copies 4*16 bytes and its baseline uses binary32/binary64. */
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "OSTC4 baseline requires IEEE binary32 float");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "OSTC4 baseline requires IEEE binary64 double");
extern float gGF_low_depth_bar;
static SLifeData life;
static SDiveSettings settings;
static ostc4_observation current;
static bool initialized;

ostc4_status ostc4_initialize(const ostc4_initial *initial)
{
    if (!initial || !isfinite(initial->surface_pressure_bar) ||
        initial->surface_pressure_bar < 0.8f || initial->surface_pressure_bar > 1.2f)
        return OSTC4_BAD_INPUT;
    memset(&life, 0, sizeof(life));
    memset(&settings, 0, sizeof(settings));
    memset(&current, 0, sizeof(current));
    life.pressure_surface_bar = initial->surface_pressure_bar;
    life.pressure_ambient_bar = initial->surface_pressure_bar;
    decom_reset_with_ambientmbar(initial->surface_pressure_bar, &life);
    life.actualGas.nitrogen_percentage = 79;
    life.actualGas.AppliedDiveMode = DIVEMODE_OC;
    settings.diveMode = DIVEMODE_OC;
    settings.gf_low = 30;
    settings.gf_high = 70;
    settings.ascentRate_meterperminute = 9;
    settings.last_stop_depth_bar = 0.3f;
    settings.input_second_to_last_stop_depth_bar = 0.6f;
    settings.input_next_stop_increment_depth_bar = 0.3f;
    settings.decogaslist[0] = life.actualGas;
    gGF_low_depth_bar = 0; /* Match zero-initialized upstream process state. */
    current.timestamp_seconds = initial->timestamp_seconds;
    current.absolute_pressure_bar = initial->surface_pressure_bar;
    memcpy(current.nitrogen_bar, life.tissue_nitrogen_bar, sizeof(current.nitrogen_bar));
    memcpy(current.helium_bar, life.tissue_helium_bar, sizeof(current.helium_bar));
    current.tissues = OSTC4_AVAILABLE;
    initialized = true;
    return OSTC4_OK;
}

ostc4_status ostc4_read(ostc4_observation *observation)
{
    if (!observation)
        return OSTC4_BAD_INPUT;
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    *observation = current;
    return OSTC4_OK;
}

ostc4_status ostc4_advance(const ostc4_sample *sample)
{
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    if (!sample || !isfinite(sample->absolute_pressure_bar) ||
        sample->absolute_pressure_bar < life.pressure_surface_bar ||
        sample->absolute_pressure_bar > life.pressure_surface_bar + 6.0f)
        return OSTC4_BAD_INPUT;
    if (sample->timestamp_seconds <= current.timestamp_seconds ||
        sample->timestamp_seconds - current.timestamp_seconds > 3600)
        return OSTC4_BAD_TIME;
    const uint32_t elapsed = sample->timestamp_seconds - current.timestamp_seconds;
    SLifeData next = life;
    SDiveSettings next_settings = settings;
    SDecoinfo result = {0};
    ostc4_observation observed = {0};
    const float old_gf_depth = gGF_low_depth_bar;
    next.pressure_ambient_bar = sample->absolute_pressure_bar;
    /* This baseline excludes wholly surface intervals from the upstream dive
     * counter; transitions include their whole duration. No recording threshold. */
    if (next.pressure_ambient_bar > life.pressure_surface_bar ||
        life.pressure_ambient_bar > life.pressure_surface_bar)
        next.dive_time_seconds_without_surface_time += elapsed;
    const float depth = (next.pressure_ambient_bar - next.pressure_surface_bar) * 10.0f;
    if (depth > next.max_depth_meter)
        next.max_depth_meter = depth;
    if (next.pressure_ambient_bar == life.pressure_ambient_bar)
        decom_tissues_exposure((int)elapsed, &next);
    else
        decom_tissues_exposure_stage_schreiner((int)elapsed, &next.actualGas,
            life.pressure_ambient_bar, next.pressure_ambient_bar,
            next.tissue_nitrogen_bar, next.tissue_helium_bar);
    buehlmann_calc_deco(&next, &next_settings, &result);
    /* Upstream ceiling consumes scratch GF left by the immediately prior planner. */
    buehlmann_ceiling_calculator(&next, &result);
    for (int i = 0; i < OSTC4_TISSUES; ++i) {
        if (!isfinite(next.tissue_nitrogen_bar[i]) || next.tissue_nitrogen_bar[i] < 0 ||
            !isfinite(next.tissue_helium_bar[i]) || next.tissue_helium_bar[i] < 0) {
            gGF_low_depth_bar = old_gf_depth;
            return OSTC4_NUMERICAL_FAILURE;
        }
    }
    if (!isfinite(result.output_ceiling_meter) || !isfinite(gGF_low_depth_bar) ||
        !isfinite(next_settings.internal__pressure_first_stop_ambient_bar_as_upper_limit_for_gf_low_otherwise_zero)) {
        gGF_low_depth_bar = old_gf_depth;
        return OSTC4_NUMERICAL_FAILURE;
    }
    observed.timestamp_seconds = sample->timestamp_seconds;
    observed.dive_seconds = next.dive_time_seconds_without_surface_time;
    observed.absolute_pressure_bar = next.pressure_ambient_bar;
    memcpy(observed.nitrogen_bar, next.tissue_nitrogen_bar, sizeof(observed.nitrogen_bar));
    memcpy(observed.helium_bar, next.tissue_helium_bar, sizeof(observed.helium_bar));
    observed.gf_anchor_ambient_bar = next_settings.internal__pressure_first_stop_ambient_bar_as_upper_limit_for_gf_low_otherwise_zero;
    observed.gf_low_depth_bar = gGF_low_depth_bar;
    observed.ndl_seconds = result.output_ndl_seconds;
    observed.tts_seconds = result.output_time_to_surface_seconds;
    observed.ceiling_meters = result.output_ceiling_meter;
    memcpy(observed.stop_seconds, result.output_stop_length_seconds, sizeof(observed.stop_seconds));
    observed.tissues = OSTC4_AVAILABLE;
    if (observed.dive_seconds >= 60 && next.pressure_ambient_bar > next.pressure_surface_bar &&
        (observed.ndl_seconds > 0 || observed.tts_seconds > 0)) {
        observed.ndl = observed.ndl_seconds >= 14400 ? OSTC4_LIMITED : OSTC4_AVAILABLE;
        observed.tts = observed.tts_seconds >= 59940 ? OSTC4_LIMITED : OSTC4_AVAILABLE;
        observed.stops = observed.ceiling = observed.tts;
        for (int i = 0; i < OSTC4_STOPS; ++i)
            if (observed.stop_seconds[i] >= 59940)
                observed.stops = OSTC4_LIMITED;
    }
    life = next;
    settings = next_settings;
    current = observed;
    return OSTC4_OK;
}

const char *ostc4_availability_name(ostc4_availability availability)
{
    switch (availability) {
    case OSTC4_UNAVAILABLE: return "unavailable";
    case OSTC4_AVAILABLE: return "available";
    case OSTC4_LIMITED: return "limited";
    case OSTC4_INVALID: return "invalid";
    }
    return "invalid";
}

#ifdef OSTC4_CONTINUOUS_EXPOSURE
#include "ostc4_exposure.h"
#include "ostc4_scratch.h"
static bool exposure_incomplete;

static void invalidate_exposure(void)
{
    exposure_incomplete = true;
    current.tissues = current.ndl = current.tts = current.ceiling = current.stops = OSTC4_INVALID;
}

ostc4_status ostc4_exposure_initialize(const ostc4_initial *initial)
{
    ostc4_status status = ostc4_initialize(initial);
    if (status == OSTC4_OK)
        exposure_incomplete = false;
    else if (initialized)
        invalidate_exposure();
    return status;
}

ostc4_status ostc4_exposure_read(ostc4_observation *observation)
{
    return ostc4_read(observation);
}

ostc4_status ostc4_exposure_missing(void)
{
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    invalidate_exposure();
    return OSTC4_OK;
}

ostc4_status ostc4_exposure_advance(const ostc4_sample *sample)
{
    if (exposure_incomplete)
        return OSTC4_BAD_INPUT;
    ostc4_status status = ostc4_advance(sample);
    if (status != OSTC4_OK && initialized)
        invalidate_exposure();
    return status;
}

ostc4_status ostc4_exposure_predict(const ostc4_sample *future, ostc4_observation *prediction)
{
    if (!prediction)
        return OSTC4_BAD_INPUT;
    memset(prediction, 0, sizeof(*prediction));
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    if (exposure_incomplete) {
        prediction->tissues = prediction->ndl = prediction->tts =
            prediction->ceiling = prediction->stops = OSTC4_INVALID;
        return OSTC4_BAD_INPUT;
    }
    const SLifeData saved_life = life;
    const SDiveSettings saved_settings = settings;
    const ostc4_observation saved_current = current;
    ostc4_scratch scratch;
    ostc4_scratch_save(&scratch);
    ostc4_status status = ostc4_advance(future);
    if (status == OSTC4_OK)
        *prediction = current;
    life = saved_life;
    settings = saved_settings;
    current = saved_current;
    ostc4_scratch_restore(&scratch);
    return status;
}
#endif
