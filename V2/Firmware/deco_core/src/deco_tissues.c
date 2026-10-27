#include "deco_tissues.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0)
#error "Tissue integration requires non-fast-math IEEE binary64"
#endif
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
               DBL_MIN_EXP == -1021 && sizeof(double) == 8,
               "Tissue integration requires IEEE binary64 double");

static const deco_tissues_constants constants = {
    "air-zhl16c-4min-gf30-70-v1",
    "zhl16c-n2-4min-table25-v1",
    "b3fca25dfabfa2425b4e0979e04bd3417006dce5b9695fbd23f63a64ed88fd2a",
    0.79, 0.0627,
    {
        {4.0, 1.2599, 0.5050}, {8.0, 1.0000, 0.6514},
        {12.5, 0.8618, 0.7222}, {18.5, 0.7562, 0.7825},
        {27.0, 0.6200, 0.8126}, {38.3, 0.5043, 0.8434},
        {54.3, 0.4410, 0.8693}, {77.0, 0.4000, 0.8910},
        {109.0, 0.3750, 0.9092}, {146.0, 0.3500, 0.9222},
        {187.0, 0.3295, 0.9319}, {239.0, 0.3065, 0.9403},
        {305.0, 0.2835, 0.9477}, {390.0, 0.2610, 0.9544},
        {498.0, 0.2480, 0.9602}, {635.0, 0.2327, 0.9653}
    }
};

const deco_tissues_constants *deco_tissues_get_constants(void)
{
    return &constants;
}

static int valid_pressure(double pressure)
{
    return isfinite(pressure) && pressure >= constants.water_vapour_bar;
}

deco_tissues_result deco_tissues_init_fresh(deco_tissues_state *state,
                                           double surface_bar_abs)
{
    deco_tissues_state candidate;
    size_t i;
    double inspired;
    if (state == NULL || !valid_pressure(surface_bar_abs)) {
        return DECO_TISSUES_ARGUMENT_ERROR;
    }
    inspired = constants.nitrogen_fraction *
               (surface_bar_abs - constants.water_vapour_bar);
    if (!isfinite(inspired) || inspired < 0.0) {
        return DECO_TISSUES_NUMERICAL_ERROR;
    }
    memset(&candidate, 0, sizeof(candidate));
    memcpy(candidate.policy_id, constants.policy_id, strlen(constants.policy_id) + 1);
    memcpy(candidate.coefficient_sha256, constants.coefficient_sha256,
           strlen(constants.coefficient_sha256) + 1);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        candidate.nitrogen_bar[i] = inspired;
    }
    memcpy(state, &candidate, sizeof(candidate));
    return DECO_TISSUES_OK;
}

static int matching_identity(const deco_tissues_state *state)
{
    /* Bounded comparisons also reject unterminated caller-owned fields. */
    return strncmp(state->policy_id, constants.policy_id,
                   sizeof(state->policy_id)) == 0 &&
           strncmp(state->coefficient_sha256, constants.coefficient_sha256,
                   sizeof(state->coefficient_sha256)) == 0;
}

deco_tissues_result deco_tissues_integrate(deco_tissues_state *state,
                                          double start_bar_abs,
                                          double end_bar_abs,
                                          uint64_t duration_ms)
{
    double candidate[DECO_TISSUE_COUNT];
    double minutes, inspired_start, inspired_end;
    size_t i;
    if (state == NULL || !valid_pressure(start_bar_abs) ||
        !valid_pressure(end_bar_abs) || duration_ms == 0) {
        return DECO_TISSUES_ARGUMENT_ERROR;
    }
    if (!matching_identity(state)) {
        return DECO_TISSUES_STATE_ERROR;
    }
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        if (!isfinite(state->nitrogen_bar[i]) || state->nitrogen_bar[i] < 0.0) {
            return DECO_TISSUES_STATE_ERROR;
        }
    }
    minutes = (double)duration_ms / 60000.0;
    inspired_start = constants.nitrogen_fraction *
                     (start_bar_abs - constants.water_vapour_bar);
    inspired_end = constants.nitrogen_fraction *
                   (end_bar_abs - constants.water_vapour_bar);
    if (!isfinite(minutes) || minutes <= 0.0 || !isfinite(inspired_start) ||
        !isfinite(inspired_end)) {
        return DECO_TISSUES_NUMERICAL_ERROR;
    }
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        const double ln2 = 0.693147180559945309417232121458176568;
        double x = ln2 * minutes / constants.compartments[i].half_time_min;
        double e = exp(-x);
        double q = -expm1(-x);
        double w;
        double start_weight;
        /* Schreiner's equation as a convex combination of initial tissue and
         * inspired endpoints: P = P0*exp(-x) + A0*(q-w) + A1*w,
         * q=1-exp(-x), w=1-q/x, x=ln(2)*minutes/half_time.
         * The series avoids cancellation in w for short measured intervals.
         * Endpoint weighting also avoids overflowing a pressure slope or delta.
         */
        if (x < 0.001) {
            w = x * (0.5 + x * (-1.0 / 6.0 + x * (1.0 / 24.0 +
                x * (-1.0 / 120.0 + x * (1.0 / 720.0 +
                x * (-1.0 / 5040.0))))));
            start_weight = q - w;
        } else {
            w = 1.0 - q / x;
            /* q-w cancels when w rounds close to one at large x. */
            start_weight = q / x - e;
        }
        if (!isfinite(x) || x <= 0.0 || !isfinite(e) || !isfinite(q) ||
            !isfinite(w) || w < 0.0 || !isfinite(start_weight) ||
            start_weight < 0.0) {
            return DECO_TISSUES_NUMERICAL_ERROR;
        }
        candidate[i] = state->nitrogen_bar[i] * e +
                       inspired_start * start_weight + inspired_end * w;
        if (!isfinite(candidate[i]) || candidate[i] < 0.0) {
            return DECO_TISSUES_NUMERICAL_ERROR;
        }
    }
    memcpy(state->nitrogen_bar, candidate, sizeof(candidate));
    return DECO_TISSUES_OK;
}
