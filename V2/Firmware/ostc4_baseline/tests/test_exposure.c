/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ostc4_exposure.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
static const ostc4_initial initial = {0, 1.01325f};
static void invalid(const ostc4_observation *out)
{
    CHECK(out->tissues == OSTC4_INVALID && out->ndl == OSTC4_INVALID);
    CHECK(out->tts == OSTC4_INVALID && out->ceiling == OSTC4_INVALID && out->stops == OSTC4_INVALID);
}
static ostc4_observation read_now(void)
{
    ostc4_observation out;
    CHECK(ostc4_exposure_read(&out) == OSTC4_OK);
    return out;
}
static void exposure_faults(void)
{
    const ostc4_sample bad[] = {{120, 4.01325f}, {119, 4.01325f}, {3721, 4.01325f},
                              {130, NAN}, {130, INFINITY}, {130, -INFINITY},
                              {130, 0.1f}, {130, 8.0f}};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
        CHECK(ostc4_exposure_advance(&(ostc4_sample){120, 4.01325f}) == OSTC4_OK);
        CHECK(ostc4_exposure_advance(&bad[i]) == (i < 3 ? OSTC4_BAD_TIME : OSTC4_BAD_INPUT));
        ostc4_observation out = read_now();
        invalid(&out);
        CHECK(out.timestamp_seconds == 120);
        CHECK(ostc4_exposure_advance(&(ostc4_sample){240, 4.01325f}) != OSTC4_OK);
        CHECK(ostc4_exposure_predict(&(ostc4_sample){240, 4.01325f}, &out) != OSTC4_OK);
        invalid(&out);
    }
    CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
    CHECK(ostc4_exposure_missing() == OSTC4_OK);
    ostc4_observation out = read_now(); invalid(&out);
    CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
    CHECK(ostc4_exposure_advance(NULL) == OSTC4_BAD_INPUT);
    out = read_now(); invalid(&out);
    CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
    CHECK(ostc4_exposure_initialize(&(ostc4_initial){0, NAN}) == OSTC4_BAD_INPUT);
    out = read_now(); invalid(&out);
    /* Fresh fixtures are the only recovery operation. Equal surface endpoints
     * still integrate: reset N2 convention differs slightly from breathing air. */
    CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
    out = read_now();
    const float fresh = out.nitrogen_bar[0];
    CHECK(out.tissues == OSTC4_AVAILABLE && out.ndl == OSTC4_UNAVAILABLE);
    CHECK(ostc4_exposure_advance(&(ostc4_sample){3600, 1.01325f}) == OSTC4_OK);
    out = read_now();
    CHECK(out.nitrogen_bar[0] < fresh && out.dive_seconds == 0);
    CHECK(out.tissues == OSTC4_AVAILABLE && out.ndl == OSTC4_UNAVAILABLE);
    /* Absolute timestamps must not wrap into an accepted new interval. */
    CHECK(ostc4_exposure_initialize(&(ostc4_initial){UINT32_MAX-120, 1.01325f}) == OSTC4_OK);
    CHECK(ostc4_exposure_advance(&(ostc4_sample){UINT32_MAX, 2.01325f}) == OSTC4_OK);
    CHECK(ostc4_exposure_advance(&(ostc4_sample){0, 2.01325f}) == OSTC4_BAD_TIME);
    out = read_now(); invalid(&out);
}
static void predictions_preserve_state(void)
{
    const ostc4_sample profile[] = {{120,4.01325f},{1920,4.01325f},{2100,1.01325f},
        {3600,1.01325f},{3720,3.01325f},{4920,3.01325f},{5040,1.01325f}};
    ostc4_observation control[7];
    for (int run = 0; run < 3; ++run) {
        CHECK(ostc4_exposure_initialize(&initial) == OSTC4_OK);
        for (unsigned i = 0; i < 7; ++i) {
            ostc4_observation before = read_now(), after, first, again;
            if (run) {
                const ostc4_sample future = {before.timestamp_seconds+3600, run == 1 ? 6.01325f : 1.21325f};
                CHECK(ostc4_exposure_predict(&future, &first) == OSTC4_OK);
                CHECK(ostc4_exposure_predict(&future, &again) == OSTC4_OK);
                CHECK(memcmp(&first, &again, sizeof(first)) == 0);
                CHECK(first.timestamp_seconds == future.timestamp_seconds);
                CHECK(first.tissues == OSTC4_AVAILABLE);
                CHECK(ostc4_exposure_predict(&(ostc4_sample){before.timestamp_seconds, NAN}, &again) == OSTC4_BAD_INPUT);
                CHECK(again.tissues == OSTC4_UNAVAILABLE && again.ndl == OSTC4_UNAVAILABLE);
                CHECK(ostc4_exposure_predict(NULL, &again) == OSTC4_BAD_INPUT);
                CHECK(ostc4_exposure_predict(&future, NULL) == OSTC4_BAD_INPUT);
                after = read_now();
                CHECK(memcmp(&before, &after, sizeof(before)) == 0);
            }
            CHECK(ostc4_exposure_advance(&profile[i]) == OSTC4_OK);
            after = read_now();
            if (!run) control[i] = after;
            else CHECK(memcmp(&control[i], &after, sizeof(after)) == 0);
        }
        CHECK(control[1].gf_anchor_ambient_bar > initial.surface_pressure_bar);
        CHECK(control[3].nitrogen_bar[0] > 0.7902f*(initial.surface_pressure_bar-0.0493f));
    }
}
int main(void)
{
    ostc4_observation out;
    CHECK(ostc4_exposure_read(&out) == OSTC4_NOT_INITIALIZED);
    CHECK(ostc4_exposure_missing() == OSTC4_NOT_INITIALIZED);
    CHECK(ostc4_exposure_predict(&(ostc4_sample){120, 4.01325f}, &out) == OSTC4_NOT_INITIALIZED);
    CHECK(out.tissues == OSTC4_UNAVAILABLE);
    exposure_faults();
    predictions_preserve_state();
    return 0;
}
