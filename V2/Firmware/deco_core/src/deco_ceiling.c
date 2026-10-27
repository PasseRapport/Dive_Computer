#include "deco_ceiling.h"
#include "deco_validation.h"

#include <float.h>
#include <stddef.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0)
#error "Ceiling calculation requires non-fast-math IEEE binary64"
#endif
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
               DBL_MIN_EXP == -1021 && sizeof(double) == 8,
               "Ceiling calculation requires IEEE binary64 double");

static deco_ceiling_result unavailable(deco_tissues_result status)
{
    deco_ceiling_result result = {0};
    size_t i;
    result.status = status;
    result.raw_max_bar_abs = NAN;
    result.local_ceiling_bar_abs = NAN;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i)
        result.raw_limits_bar_abs[i] = NAN;
    return result;
}

deco_ceiling_result deco_ceiling_fixed(const deco_tissues_state *state,
                                     double surface_bar_abs, double gf)
{
    const deco_tissues_constants *c = deco_tissues_get_constants();
    deco_ceiling_result result = {0};
    size_t i;
    if (state == NULL || !deco_valid_pressure(surface_bar_abs) ||
        !isfinite(gf) || gf <= 0.0 || gf > 1.0)
        return unavailable(DECO_TISSUES_ARGUMENT_ERROR);
    if (!deco_valid_state(state)) return unavailable(DECO_TISSUES_STATE_ERROR);
    result.raw_max_bar_abs = -INFINITY;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        double limit = (state->nitrogen_bar[i] - gf * c->compartments[i].a_bar) /
                       (1.0 - gf + gf / c->compartments[i].b);
        if (!isfinite(limit)) return unavailable(DECO_TISSUES_NUMERICAL_ERROR);
        result.raw_limits_bar_abs[i] = limit;
        if (limit > result.raw_max_bar_abs) result.raw_max_bar_abs = limit;
    }
    result.local_ceiling_bar_abs = fmax(surface_bar_abs, result.raw_max_bar_abs);
    result.status = DECO_TISSUES_OK;
    return result;
}

deco_gf_result deco_ceiling_select_gf(double current_bar_abs,
                                    double surface_bar_abs,
                                    const double *retained_anchor_bar_abs)
{
    deco_gf_result result = {DECO_TISSUES_ARGUMENT_ERROR, NAN};
    double anchor;
    if (!deco_valid_pressure(current_bar_abs) || !deco_valid_pressure(surface_bar_abs))
        return result;
    if (retained_anchor_bar_abs == NULL) {
        result.gf = 0.70;
    } else {
        anchor = *retained_anchor_bar_abs;
        if (!isfinite(anchor) || anchor <= surface_bar_abs) return result;
        if (current_bar_abs >= anchor) result.gf = 0.30;
        else if (current_bar_abs <= surface_bar_abs) result.gf = 0.70;
        else result.gf = 0.30 + 0.40 *
            ((anchor - current_bar_abs) / (anchor - surface_bar_abs));
    }
    result.status = DECO_TISSUES_OK;
    return result;
}

deco_violation_result deco_ceiling_check_violation(
    const deco_ceiling_result *ceiling, double current_bar_abs)
{
    deco_violation_result result = {DECO_TISSUES_ARGUMENT_ERROR, false};
    if (ceiling == NULL || !deco_valid_pressure(current_bar_abs)) return result;
    if (ceiling->status != DECO_TISSUES_OK ||
        !deco_valid_pressure(ceiling->local_ceiling_bar_abs)) {
        result.status = DECO_TISSUES_STATE_ERROR;
        return result;
    }
    result.status = DECO_TISSUES_OK;
    result.violated = current_bar_abs + 1e-9 < ceiling->local_ceiling_bar_abs;
    return result;
}
