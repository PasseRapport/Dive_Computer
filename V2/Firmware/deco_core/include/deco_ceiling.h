#ifndef DECO_CEILING_H
#define DECO_CEILING_H

#include "deco_tissues.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    deco_tissues_result status;
    double raw_limits_bar_abs[DECO_TISSUE_COUNT];
    double raw_max_bar_abs;
    double local_ceiling_bar_abs;
} deco_ceiling_result;

typedef struct {
    deco_tissues_result status;
    double gf;
} deco_gf_result;

typedef struct {
    deco_tissues_result status;
    bool violated;
} deco_violation_result;

/* Pure local pressure constraint, not authorization for direct ascent.
 * Requires compatible finite nonnegative tissues, finite surface >= 0.0627
 * bar absolute and 0 < gf <= 1. Retains negative raw limits; only the local
 * ceiling is surface-clamped. All arithmetic uses binary64 without rounding.
 * Always check status before using results. On error every numerical output
 * is NaN, so a previous valid ceiling cannot survive a failed calculation.
 * Inputs are never modified. No schedule or anchor lifecycle is inferred. */
deco_ceiling_result deco_ceiling_fixed(const deco_tissues_state *state,
                                     double surface_bar_abs, double gf);

/* NULL retained_anchor_bar_abs means absent: return GF high (0.70).
 * Otherwise it must be finite and strictly greater than surface. At/deeper
 * than anchor use 0.30; at/shallower than surface use 0.70; interpolate
 * linearly in absolute ambient pressure between these endpoints. Current
 * and surface pressures must be finite and >= 0.0627 bar. No anchor mutation.
 * On error gf is NaN. */
deco_gf_result deco_ceiling_select_gf(double current_bar_abs,
                                    double surface_bar_abs,
                                    const double *retained_anchor_bar_abs);

/* Requires an explicitly successful ceiling result and finite current >=
 * 0.0627 bar. Exactly current_bar_abs + 1e-9 < local_ceiling_bar_abs.
 * On error violated is false and unavailable: callers MUST check status.
 * This flag is a local constraint, never schedule availability. */
deco_violation_result deco_ceiling_check_violation(
    const deco_ceiling_result *ceiling, double current_bar_abs);

#ifdef __cplusplus
}
#endif
#endif
