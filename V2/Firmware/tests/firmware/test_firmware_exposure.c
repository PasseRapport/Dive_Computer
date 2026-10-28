/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "firmware_exposure.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    firmware_exposure_result result;
    assert(firmware_exposure_read(&result) == OSTC4_NOT_INITIALIZED);
    assert(!result.synthetic && result.calculation.tissues == OSTC4_UNAVAILABLE);
    assert(firmware_exposure_start_synthetic(0, 101325) == OSTC4_OK);
    assert(firmware_exposure_sample(120000, 401325, true) == OSTC4_OK);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.synthetic);
    assert(result.calculation.timestamp_seconds == 120);
    assert(fabsf(result.calculation.absolute_pressure_bar - 4.01325f) < 0.000001f);
    assert(result.calculation.tissues == OSTC4_AVAILABLE);
    /* Fractional seconds must not be rounded into apparently complete history. */
    assert(firmware_exposure_sample(121001, 401325, true) == OSTC4_BAD_TIME);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.calculation.tissues == OSTC4_INVALID);
    assert(firmware_exposure_sample(122000, 401325, true) != OSTC4_OK);
    assert(firmware_exposure_start_synthetic(0, 101325) == OSTC4_OK);
    assert(firmware_exposure_sample(1000, 101325, false) == OSTC4_BAD_INPUT);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.calculation.ndl == OSTC4_INVALID);
    assert(firmware_exposure_start_synthetic(0, 101325) == OSTC4_OK);
    assert(firmware_exposure_sample(((uint64_t)UINT32_MAX + 1) * 1000,
                                    101325, true) == OSTC4_BAD_TIME);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.calculation.tissues == OSTC4_INVALID);
    assert(firmware_exposure_start_synthetic(0, 101325) == OSTC4_OK);
    assert(firmware_exposure_start_synthetic(1, 101325) == OSTC4_BAD_TIME);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.calculation.tissues == OSTC4_INVALID);
    assert(firmware_exposure_smoke() == 0);
    assert(firmware_exposure_read(&result) == OSTC4_OK);
    assert(result.synthetic && result.calculation.tissues == OSTC4_INVALID);
    puts("FIRMWARE PRESSURE/TIME PASS (synthetic host execution)");
}
