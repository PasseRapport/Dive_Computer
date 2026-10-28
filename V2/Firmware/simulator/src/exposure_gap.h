/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Caller-owned missing-data timing. Knowing duration never establishes pressure
 * exposure. ongoing means the input signal is still absent; a completed gap can
 * leave physiological guidance invalid indefinitely. */
struct exposure_gap {
    bool present, time_known, ongoing;
    uint32_t start_seconds, duration_seconds;
};
