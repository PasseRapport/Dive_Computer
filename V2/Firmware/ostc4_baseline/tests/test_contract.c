/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ostc4_baseline.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
int main(void)
{
    ostc4_observation out;
    const ostc4_initial initial = {100, 1.01325f};
    CHECK(ostc4_read(&out) == OSTC4_NOT_INITIALIZED);
    CHECK(ostc4_initialize(&initial) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.timestamp_seconds == 100);
    CHECK(out.dive_seconds == 0);
    CHECK(out.tissues == OSTC4_AVAILABLE);
    CHECK(out.ndl == OSTC4_UNAVAILABLE);
    CHECK(out.tts == OSTC4_UNAVAILABLE);
    for (int i = 0; i < OSTC4_TISSUES; ++i) {
        CHECK(out.nitrogen_bar[i] > 0);
        CHECK(out.helium_bar[i] == 0);
    }
    const float fresh_nitrogen = out.nitrogen_bar[0];
    const ostc4_sample descent = {130, 2.01325f};
    CHECK(ostc4_advance(&descent) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.timestamp_seconds == 130);
    CHECK(out.nitrogen_bar[0] > fresh_nitrogen);
    CHECK(out.ndl == OSTC4_UNAVAILABLE);
    CHECK(out.tts == OSTC4_UNAVAILABLE);
    CHECK(out.ceiling == OSTC4_UNAVAILABLE);
    CHECK(out.stops == OSTC4_UNAVAILABLE);
    ostc4_observation before = out;
    const ostc4_sample duplicate = {130, 2.01325f};
    CHECK(ostc4_advance(&duplicate) == OSTC4_BAD_TIME);
    const ostc4_sample nan_sample = {140, NAN};
    CHECK(ostc4_advance(&nan_sample) == OSTC4_BAD_INPUT);
    const ostc4_sample gap = {4000, 2.01325f};
    CHECK(ostc4_advance(&gap) == OSTC4_BAD_TIME);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(memcmp(&before, &out, sizeof(out)) == 0);
    const ostc4_sample shallow = {700, 2.01325f};
    CHECK(ostc4_advance(&shallow) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.ndl == OSTC4_AVAILABLE || out.ndl == OSTC4_LIMITED);
    CHECK(out.ndl_seconds > 0);
    CHECK(out.tts == OSTC4_AVAILABLE && out.tts_seconds == 0);
    CHECK(out.ceiling == OSTC4_AVAILABLE && out.ceiling_meters == 0);
    CHECK(out.stops == OSTC4_AVAILABLE);
    CHECK(ostc4_initialize(&initial) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.gf_anchor_ambient_bar == 0 && out.gf_low_depth_bar == 0);
    const ostc4_sample deep = {220, 4.01325f};
    const ostc4_sample bottom = {2320, 4.01325f};
    CHECK(ostc4_advance(&deep) == OSTC4_OK);
    CHECK(ostc4_advance(&bottom) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.tts == OSTC4_AVAILABLE && out.tts_seconds > 0);
    CHECK(out.ndl == OSTC4_AVAILABLE && out.ndl_seconds == 0);
    CHECK(out.ceiling == OSTC4_AVAILABLE && out.ceiling_meters > 0);
    CHECK(out.gf_anchor_ambient_bar > initial.surface_pressure_bar);
    unsigned int stop_total = 0;
    for (int i = 0; i < OSTC4_STOPS; ++i) stop_total += out.stop_seconds[i];
    CHECK(stop_total > 0);
    CHECK(out.tts_seconds >= (int)stop_total);
    CHECK(ostc4_initialize(&initial) == OSTC4_OK);
    const ostc4_sample very_shallow = {160, 1.11325f};
    CHECK(ostc4_advance(&very_shallow) == OSTC4_OK);
    CHECK(ostc4_read(&out) == OSTC4_OK);
    CHECK(out.ndl == OSTC4_LIMITED && out.ndl_seconds == 14400);
    CHECK(ostc4_initialize(NULL) == OSTC4_BAD_INPUT);
    CHECK(ostc4_read(NULL) == OSTC4_BAD_INPUT);
    CHECK(ostc4_advance(NULL) == OSTC4_BAD_INPUT);
    return 0;
}
