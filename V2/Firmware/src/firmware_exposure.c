/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "firmware_exposure.h"
#include <math.h>
#include <string.h>

static bool synthetic;

static bool whole_seconds(uint64_t milliseconds)
{
    return milliseconds % 1000 == 0 && milliseconds / 1000 <= UINT32_MAX;
}

static ostc4_status reject(ostc4_status status)
{
    (void)ostc4_exposure_missing();
    return status;
}

ostc4_status firmware_exposure_start_synthetic(uint64_t timestamp_ms, uint32_t surface_pa)
{
    if (!whole_seconds(timestamp_ms))
        return reject(OSTC4_BAD_TIME);
    const ostc4_initial initial = {(uint32_t)(timestamp_ms / 1000), surface_pa / 100000.0f};
    const ostc4_status status = ostc4_exposure_initialize(&initial);
    if (status == OSTC4_OK)
        synthetic = true;
    return status;
}

ostc4_status firmware_exposure_sample(uint64_t timestamp_ms, uint32_t absolute_pa, bool valid)
{
    if (!synthetic)
        return OSTC4_NOT_INITIALIZED;
    if (!valid)
        return reject(OSTC4_BAD_INPUT);
    if (!whole_seconds(timestamp_ms))
        return reject(OSTC4_BAD_TIME);
    const ostc4_sample sample = {(uint32_t)(timestamp_ms / 1000), absolute_pa / 100000.0f};
    return ostc4_exposure_advance(&sample);
}

ostc4_status firmware_exposure_read(firmware_exposure_result *result)
{
    if (!result)
        return OSTC4_BAD_INPUT;
    memset(result, 0, sizeof(*result));
    result->synthetic = synthetic;
    return ostc4_exposure_read(&result->calculation);
}

static int run_smoke(void)
{
    firmware_exposure_result result;
    ostc4_observation before, prediction, after;
    if (firmware_exposure_start_synthetic(0, 101325) != OSTC4_OK ||
        firmware_exposure_read(&result) != OSTC4_OK || !result.synthetic)
        return 1;
    /* Independent arithmetic: .7902 * (1.01325 - .0493) = .76171329 bar.
     * 2e-7 bar was fixed before target compilation; this is a wiring smoke
     * check, not independent schedule acceptance or target runtime evidence. */
    for (unsigned i = 0; i < OSTC4_TISSUES; ++i)
        if (fabsf(result.calculation.nitrogen_bar[i] - 0.76171329f) > 2e-7f ||
            result.calculation.helium_bar[i] != 0.0f)
            return 2;
    if (firmware_exposure_sample(120000, 401325, true) != OSTC4_OK ||
        firmware_exposure_read(&result) != OSTC4_OK ||
        result.calculation.timestamp_seconds != 120 ||
        fabsf(result.calculation.absolute_pressure_bar - 4.01325f) > 1e-6f ||
        result.calculation.tissues != OSTC4_AVAILABLE)
        return 3;
    before = result.calculation;
    const ostc4_sample future = {240, 4.01325f};
    if (ostc4_exposure_predict(&future, &prediction) != OSTC4_OK ||
        prediction.timestamp_seconds != 240 ||
        ostc4_exposure_read(&after) != OSTC4_OK ||
        memcmp(&before, &after, sizeof(before)) != 0)
        return 4;
    if (firmware_exposure_sample(121000, 401325, false) != OSTC4_BAD_INPUT ||
        firmware_exposure_read(&result) != OSTC4_OK ||
        result.calculation.tissues != OSTC4_INVALID ||
        result.calculation.ndl != OSTC4_INVALID || result.calculation.tts != OSTC4_INVALID ||
        result.calculation.ceiling != OSTC4_INVALID || result.calculation.stops != OSTC4_INVALID)
        return 5;
    return 0;
}

int firmware_exposure_smoke(void)
{
    const int status = run_smoke();
    (void)ostc4_exposure_missing();
    return status;
}
