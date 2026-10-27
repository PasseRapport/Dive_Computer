#include "deco_ceiling.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; \
} } while (0)

static void unavailable(deco_ceiling_result result, deco_tissues_result status)
{
    size_t i;
    CHECK(result.status == status);
    CHECK(isnan(result.raw_max_bar_abs));
    CHECK(isnan(result.local_ceiling_bar_abs));
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) CHECK(isnan(result.raw_limits_bar_abs[i]));
    CHECK(deco_ceiling_check_violation(&result, 1.0).status != DECO_TISSUES_OK);
}

static void check_fixed(const deco_tissues_state *state, double surface, double gf)
{
    deco_tissues_state before;
    deco_ceiling_result a, b;
    size_t i;
    memcpy(&before, state, sizeof(before));
    a = deco_ceiling_fixed(state, surface, gf);
    b = deco_ceiling_fixed(state, surface, gf);
    CHECK(a.status == DECO_TISSUES_OK && b.status == a.status);
    CHECK(memcmp(&before, state, sizeof(before)) == 0);
    CHECK(memcmp(a.raw_limits_bar_abs, b.raw_limits_bar_abs, sizeof(a.raw_limits_bar_abs)) == 0);
    CHECK(a.raw_max_bar_abs == b.raw_max_bar_abs);
    CHECK(a.local_ceiling_bar_abs == b.local_ceiling_bar_abs);
    CHECK(a.local_ceiling_bar_abs >= surface);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        CHECK(isfinite(a.raw_limits_bar_abs[i]));
        CHECK(a.raw_max_bar_abs >= a.raw_limits_bar_abs[i]);
    }
}

static void fixed_cases(void)
{
    const deco_tissues_constants *c = deco_tissues_get_constants();
    const double gfs[] = {0.30, 0.70, 0.50, 1.0};
    deco_tissues_state state;
    size_t g, i;
    CHECK(deco_tissues_init_fresh(&state, 1.01325) == DECO_TISSUES_OK);
    for (g = 0; g < 4; ++g) {
        deco_ceiling_result r = deco_ceiling_fixed(&state, 1.01325, gfs[g]);
        check_fixed(&state, 1.01325, gfs[g]);
        CHECK(r.local_ceiling_bar_abs == 1.01325);
    }
    CHECK(deco_tissues_integrate(&state, 4.01325, 4.01325, 1800000) == DECO_TISSUES_OK);
    for (g = 0; g < 4; ++g) check_fixed(&state, 1.01325, gfs[g]);
    for (g = 0; g < 4; ++g) {
        /* Reverse the constraint analytically to construct equal-limit and
         * individually controlling states for every compartment. */
        double gf = gfs[g];
        for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
            state.nitrogen_bar[i] = 2.0 * (1.0 - gf + gf / c->compartments[i].b) +
                                    gf * c->compartments[i].a_bar;
        }
        check_fixed(&state, 1.0, gf);
        deco_ceiling_result equal = deco_ceiling_fixed(&state, 1.0, gf);
        CHECK(fabs(equal.raw_max_bar_abs - 2.0) <= 1e-15);
        for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
            double original = state.nitrogen_bar[i];
            state.nitrogen_bar[i] += 1.0;
            deco_ceiling_result r = deco_ceiling_fixed(&state, 1.0, gf);
            CHECK(r.raw_max_bar_abs == r.raw_limits_bar_abs[i]);
            CHECK(r.raw_max_bar_abs > 2.0);
            check_fixed(&state, 1.0, gf);
            state.nitrogen_bar[i] = original;
        }
    }
    /* An exact binary64 tie across all 16 compartments at GF=1. */
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) state.nitrogen_bar[i] = c->compartments[i].a_bar;
    deco_ceiling_result tie = deco_ceiling_fixed(&state, 0.0627, 1.0);
    CHECK(tie.raw_max_bar_abs == 0.0 && tie.local_ceiling_bar_abs == 0.0627);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) CHECK(tie.raw_limits_bar_abs[i] == 0.0);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) state.nitrogen_bar[i] = 0.0;
    deco_ceiling_result negative = deco_ceiling_fixed(&state, 0.0627, 0.7);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) CHECK(negative.raw_limits_bar_abs[i] < 0.0);
    CHECK(negative.status == DECO_TISSUES_OK && negative.local_ceiling_bar_abs == 0.0627);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) state.nitrogen_bar[i] = DBL_MAX;
    check_fixed(&state, DBL_MAX, nextafter(0.0, 1.0));
}

static void invalid_cases(void)
{
    const double pressures[] = {NAN, INFINITY, -INFINITY, 0.0, -1.0, 0.0626};
    const double tissues[] = {NAN, INFINITY, -INFINITY, -1.0};
    const double gfs[] = {NAN, INFINITY, -INFINITY, -1.0, 0.0, 1.00001};
    deco_tissues_state state, before;
    deco_ceiling_result r;
    size_t i, j;
    CHECK(deco_tissues_init_fresh(&state, 1.0) == DECO_TISSUES_OK);
    r = deco_ceiling_fixed(&state, 1.0, 0.7);
    CHECK(r.status == DECO_TISSUES_OK);
    r = deco_ceiling_fixed(NULL, 1.0, 0.7);
    unavailable(r, DECO_TISSUES_ARGUMENT_ERROR);
    memcpy(&before, &state, sizeof(state));
    for (i = 0; i < sizeof(pressures) / sizeof(pressures[0]); ++i)
        unavailable(deco_ceiling_fixed(&state, pressures[i], 0.7), DECO_TISSUES_ARGUMENT_ERROR);
    for (i = 0; i < sizeof(gfs) / sizeof(gfs[0]); ++i)
        unavailable(deco_ceiling_fixed(&state, 1.0, gfs[i]), DECO_TISSUES_ARGUMENT_ERROR);
    CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        for (j = 0; j < sizeof(tissues) / sizeof(tissues[0]); ++j) {
            CHECK(deco_tissues_init_fresh(&state, 1.0) == DECO_TISSUES_OK);
            state.nitrogen_bar[i] = tissues[j];
            memcpy(&before, &state, sizeof(state));
            unavailable(deco_ceiling_fixed(&state, 1.0, 0.7), DECO_TISSUES_STATE_ERROR);
            CHECK(memcmp(&before, &state, sizeof(state)) == 0);
        }
    }
    for (i = 0; i < 4; ++i) {
        CHECK(deco_tissues_init_fresh(&state, 1.0) == DECO_TISSUES_OK);
        if (i == 0) state.policy_id[0] ^= 1;
        if (i == 1) memset(state.policy_id, 'x', sizeof(state.policy_id));
        if (i == 2) state.coefficient_sha256[0] ^= 1;
        if (i == 3) memset(state.coefficient_sha256, 'x', sizeof(state.coefficient_sha256));
        memcpy(&before, &state, sizeof(state));
        unavailable(deco_ceiling_fixed(&state, 1.0, 0.7), DECO_TISSUES_STATE_ERROR);
        CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    }
}

static void gf_cases(void)
{
    const double current[] = {0.0627, 1, 1.5, 2, 2.5, 3, 4};
    const double expected[] = {0.7, 0.7, 0.6, 0.5, 0.4, 0.3, 0.3};
    const double invalid[] = {NAN, INFINITY, -INFINITY, -1, 0, 0.0626};
    const double anchors[] = {NAN, INFINITY, -INFINITY, -1, 0, 0.0627, 1};
    double anchor = 3, before;
    size_t i;
    memcpy(&before, &anchor, sizeof(anchor));
    for (i = 0; i < sizeof(current) / sizeof(current[0]); ++i) {
        deco_gf_result r = deco_ceiling_select_gf(current[i], 1, &anchor);
        deco_gf_result again = deco_ceiling_select_gf(current[i], 1, &anchor);
        CHECK(r.status == DECO_TISSUES_OK && fabs(r.gf - expected[i]) <= 1e-15);
        CHECK(again.status == r.status && again.gf == r.gf);
        CHECK(memcmp(&before, &anchor, sizeof(anchor)) == 0);
        r = deco_ceiling_select_gf(current[i], 1, NULL);
        CHECK(r.status == DECO_TISSUES_OK && r.gf == 0.7);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        deco_gf_result a = deco_ceiling_select_gf(invalid[i], 1, &anchor);
        deco_gf_result b = deco_ceiling_select_gf(2, invalid[i], NULL);
        CHECK(a.status == DECO_TISSUES_ARGUMENT_ERROR && isnan(a.gf));
        CHECK(b.status == DECO_TISSUES_ARGUMENT_ERROR && isnan(b.gf));
    }
    for (i = 0; i < sizeof(anchors) / sizeof(anchors[0]); ++i) {
        double bad = anchors[i], copy;
        memcpy(&copy, &bad, sizeof(bad));
        deco_gf_result r = deco_ceiling_select_gf(1, 1, &bad);
        CHECK(r.status == DECO_TISSUES_ARGUMENT_ERROR && isnan(r.gf));
        CHECK(memcmp(&copy, &bad, sizeof(bad)) == 0);
    }
    anchor = DBL_MAX;
    deco_gf_result large = deco_ceiling_select_gf(DBL_MAX / 2, 0.0627, &anchor);
    CHECK(large.status == DECO_TISSUES_OK && fabs(large.gf - 0.5) <= 1e-15);
    anchor = nextafter(1.0, INFINITY);
    CHECK(deco_ceiling_select_gf(1, 1, &anchor).gf == 0.7);
    CHECK(deco_ceiling_select_gf(anchor, 1, &anchor).gf == 0.3);
}

static void violation_cases(void)
{
    deco_ceiling_result ceiling = {0}, before;
    const double current = 2.0;
    /* Build the representable boundary from the expression itself. Decimal
     * 1e-9 is not exact and subtraction would give a different rounding path. */
    const double threshold = current + 1e-9;
    const double levels[] = {current, nextafter(threshold, -INFINITY), threshold,
                              nextafter(threshold, INFINITY)};
    const double invalid[] = {NAN, INFINITY, -INFINITY, -1, 0, 0.0626};
    size_t i;
    CHECK(threshold > current);
    CHECK(deco_ceiling_check_violation(NULL, current).status == DECO_TISSUES_ARGUMENT_ERROR);
    for (i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        ceiling.local_ceiling_bar_abs = levels[i];
        memcpy(&before, &ceiling, sizeof(ceiling));
        deco_violation_result r = deco_ceiling_check_violation(&ceiling, current);
        CHECK(r.status == DECO_TISSUES_OK && r.violated == (i == 3));
        CHECK(memcmp(&before, &ceiling, sizeof(ceiling)) == 0);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        deco_violation_result r = deco_ceiling_check_violation(&ceiling, invalid[i]);
        CHECK(r.status == DECO_TISSUES_ARGUMENT_ERROR && !r.violated);
        ceiling.local_ceiling_bar_abs = invalid[i];
        r = deco_ceiling_check_violation(&ceiling, current);
        CHECK(r.status == DECO_TISSUES_STATE_ERROR && !r.violated);
    }
    ceiling.local_ceiling_bar_abs = 3;
    for (i = 1; i <= 3; ++i) {
        ceiling.status = (deco_tissues_result)i;
        deco_violation_result r = deco_ceiling_check_violation(&ceiling, current);
        CHECK(r.status == DECO_TISSUES_STATE_ERROR && !r.violated);
    }
}

int main(void)
{
    fixed_cases(); invalid_cases(); gf_cases(); violation_cases();
    if (!failures) puts("Ceiling, GF, binary64 boundary and immutable-input checks passed");
    return failures ? 1 : 0;
}
