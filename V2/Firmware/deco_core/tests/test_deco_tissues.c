#include "deco_tissues.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; } } while (0)

/* Independent fixed transcription of all 48 accepted manifest values. These
 * expectations must never be obtained from production constants at runtime. */
static const double accepted[16][3] = {
    {4.0, 1.2599, 0.5050}, {8.0, 1.0000, 0.6514},
    {12.5, 0.8618, 0.7222}, {18.5, 0.7562, 0.7825},
    {27.0, 0.6200, 0.8126}, {38.3, 0.5043, 0.8434},
    {54.3, 0.4410, 0.8693}, {77.0, 0.4000, 0.8910},
    {109.0, 0.3750, 0.9092}, {146.0, 0.3500, 0.9222},
    {187.0, 0.3295, 0.9319}, {239.0, 0.3065, 0.9403},
    {305.0, 0.2835, 0.9477}, {390.0, 0.2610, 0.9544},
    {498.0, 0.2480, 0.9602}, {635.0, 0.2327, 0.9653}
};

static void coefficient_gate(void)
{
    const deco_tissues_constants *c = deco_tissues_get_constants();
    size_t i;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        CHECK(c->compartments[i].half_time_min == accepted[i][0]);
        CHECK(c->compartments[i].a_bar == accepted[i][1]);
        CHECK(c->compartments[i].b == accepted[i][2]);
    }
}

static void close_tissues(const deco_tissues_state *actual,
                          const double expected[DECO_TISSUE_COUNT],
                          double absolute, double relative, const char *fixture)
{
    size_t i;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        double error = fabs(actual->nitrogen_bar[i] - expected[i]);
        double tolerance = absolute + relative * fabs(expected[i]);
        if (!isfinite(error) || error > tolerance) {
            fprintf(stderr, "%s tissue %zu: actual %.17g expected %.17g error %.17g tolerance %.17g\n",
                    fixture, i + 1, actual->nitrogen_bar[i], expected[i], error, tolerance);
            ++failures;
        }
    }
}

static void analytical(void)
{
    const deco_tissues_constants *c = deco_tissues_get_constants();
    deco_tissues_state state;
    double expected[DECO_TISSUE_COUNT];
    size_t i;
    CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) expected[i] = 0.7509345;
    close_tissues(&state, expected, 1e-15, 0, "fresh baseline");
    CHECK(deco_tissues_integrate(&state, 1.01325, 1.01325, UINT64_MAX) == DECO_TISSUES_OK);
    close_tissues(&state, expected, 1e-14, 0, "equilibrium long hold");
    CHECK(deco_tissues_integrate(&state, 4.0, 4.0, 240000) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        double initial = 0.7509345;
        double inspired = 3.110467;
        expected[i] = inspired + (initial - inspired) *
                      pow(2.0, -4.0 / c->compartments[i].half_time_min);
    }
    close_tissues(&state, expected, 1e-14, 1e-14, "analytical loading hold");
    CHECK(fabs(state.nitrogen_bar[0] - (0.7509345 + 3.110467) / 2) < 1e-14);
    CHECK(deco_tissues_integrate(&state, 1.01325, 1.01325, 240000) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        expected[i] = 0.7509345 + (expected[i] - 0.7509345) *
                      pow(2.0, -4.0 / c->compartments[i].half_time_min);
    }
    close_tissues(&state, expected, 1e-14, 1e-14, "analytical elimination hold");
    CHECK(deco_tissues_init_fresh(&state, 0.0627) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) expected[i] = 0;
    close_tissues(&state, expected, 0, 0, "mathematical zero boundary");
    CHECK(deco_tissues_integrate(&state, 0.0627, 4.0627, 240000) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        double k = log(2.0) / c->compartments[i].half_time_min;
        expected[i] = 0.79 * (4.0 - (1.0 - exp(-4 * k)) / k);
    }
    close_tissues(&state, expected, 1e-13, 1e-13, "analytical linear ramp");
    /* At UINT64_MAX ms exp(-x) is zero to binary64 accuracy. A ramp from
     * enormous inspired pressure to zero has P = A_start / x; evaluating
     * q-w instead of q/x-exp(-x) loses the small surviving start weight. */
    CHECK(deco_tissues_init_fresh(&state, 0.0627) == DECO_TISSUES_OK);
    CHECK(deco_tissues_integrate(&state, 1e200, 0.0627, UINT64_MAX) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        double x = log(2.0) * ((double)UINT64_MAX / 60000.0) / accepted[i][0];
        expected[i] = (0.79 * (1e200 - 0.0627)) / x;
    }
    close_tissues(&state, expected, 0, 1e-14, "large-duration high-pressure ramp");
}

static void rejected(deco_tissues_state *state, double start, double end,
                     uint64_t duration, deco_tissues_result error)
{
    unsigned char before[sizeof(*state)];
    memcpy(before, state, sizeof(before));
    CHECK(deco_tissues_integrate(state, start, end, duration) == error);
    CHECK(memcmp(before, state, sizeof(before)) == 0);
}

static void transactional(void)
{
    deco_tissues_state state;
    unsigned char before[sizeof(state)];
    const double invalid[] = {NAN, INFINITY, -INFINITY, -1, 0, 0.0626};
    size_t i;
    CHECK(deco_tissues_init_fresh(NULL, 1.0) == DECO_TISSUES_ARGUMENT_ERROR);
    CHECK(deco_tissues_integrate(NULL, 1.0, 2.0, 1) == DECO_TISSUES_ARGUMENT_ERROR);
    memset(&state, 0xa5, sizeof(state));
    memcpy(before, &state, sizeof(state));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(deco_tissues_init_fresh(&state, invalid[i]) == DECO_TISSUES_ARGUMENT_ERROR);
        CHECK(memcmp(before, &state, sizeof(state)) == 0);
    }
    CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        rejected(&state, invalid[i], 2, 1, DECO_TISSUES_ARGUMENT_ERROR);
        rejected(&state, 2, invalid[i], 1, DECO_TISSUES_ARGUMENT_ERROR);
    }
    rejected(&state, 1, 2, 0, DECO_TISSUES_ARGUMENT_ERROR);
    state.policy_id[0] ^= 1;
    rejected(&state, 1, 2, 1, DECO_TISSUES_STATE_ERROR);
    CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
    memset(state.coefficient_sha256, 'x', sizeof(state.coefficient_sha256));
    rejected(&state, 1, 2, 1, DECO_TISSUES_STATE_ERROR);
    CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
    memset(state.policy_id, 'x', sizeof(state.policy_id));
    rejected(&state, 1, 2, 1, DECO_TISSUES_STATE_ERROR);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        const double invalid_tissue[] = {NAN, INFINITY, -INFINITY, -1.0};
        size_t j;
        for (j = 0; j < sizeof(invalid_tissue) / sizeof(invalid_tissue[0]); ++j) {
            CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
            state.nitrogen_bar[i] = invalid_tissue[j];
            rejected(&state, 1, 2, 1, DECO_TISSUES_STATE_ERROR);
        }
    }
    /* Exercise finite extreme inputs without introducing arbitrary depth/time
     * limits. Their convex solution remains finite for all 16 compartments. */
    CHECK(deco_tissues_init_fresh(&state, DBL_MAX) == DECO_TISSUES_OK);
    CHECK(deco_tissues_integrate(&state, DBL_MAX, 0.0627, UINT64_MAX) == DECO_TISSUES_OK);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) CHECK(isfinite(state.nitrogen_bar[i]));
}

static void partition_and_repeat(void)
{
    const struct { double start, end; uint64_t milliseconds; unsigned parts; } cases[] = {
        {1.01325, 4.0, 180000, 180}, {4.0, 1.01325, 180000, 180},
        {1.01325, 4.0, 60000, 60000}, {4.0, 1.01325, 60000, 60000},
        {1.01325, 1.01325, 60000, 1000}, {1.01325, 4.0, 1, 1}
    };
    size_t c;
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        deco_tissues_state whole, partition, repeat;
        unsigned part;
        CHECK(deco_tissues_init_fresh(&whole, 2.5) == DECO_TISSUES_OK);
        memcpy(&partition, &whole, sizeof(whole));
        memcpy(&repeat, &whole, sizeof(whole));
        CHECK(deco_tissues_integrate(&whole, cases[c].start, cases[c].end,
                                     cases[c].milliseconds) == DECO_TISSUES_OK);
        CHECK(deco_tissues_integrate(&repeat, cases[c].start, cases[c].end,
                                     cases[c].milliseconds) == DECO_TISSUES_OK);
        CHECK(memcmp(&whole, &repeat, sizeof(whole)) == 0);
        for (part = 0; part < cases[c].parts; ++part) {
            double start = cases[c].start + (cases[c].end - cases[c].start) *
                           (double)part / cases[c].parts;
            double end = cases[c].start + (cases[c].end - cases[c].start) *
                         (double)(part + 1) / cases[c].parts;
            CHECK(deco_tissues_integrate(&partition, start, end,
                    cases[c].milliseconds / cases[c].parts) == DECO_TISSUES_OK);
        }
        close_tissues(&partition, whole.nitrogen_bar, 1e-7, 1e-7, "partition");
    }
}

int main(void)
{
    coefficient_gate();
    analytical();
    transactional();
    partition_and_repeat();
    if (failures) fprintf(stderr, "%d tissue checks failed\n", failures);
    else puts("All 16-compartment analytical, rejection, partition and determinism checks passed");
    return failures ? 1 : 0;
}
