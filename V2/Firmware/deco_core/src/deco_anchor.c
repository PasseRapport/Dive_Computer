#include "deco_anchor.h"
#include "deco_validation.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static const double tolerance = 1e-9;
static int overlaps(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}
static deco_anchor_status mapped(deco_transit_status status)
{
    switch (status) {
    case DECO_TRANSIT_ARGUMENT_ERROR: return DECO_ANCHOR_ARGUMENT_ERROR;
    case DECO_TRANSIT_STATE_ERROR: return DECO_ANCHOR_STATE_ERROR;
    case DECO_TRANSIT_LIMIT: return DECO_ANCHOR_LIMIT;
    default: return DECO_ANCHOR_NUMERICAL_ERROR;
    }
}
static deco_transit_status query(const deco_tissues_state *staged, double current,
    double destination, double surface, const double *anchor, int fixed,
    const deco_anchor_allowance *budget, deco_anchor_workspace *workspace,
    deco_anchor_result *r)
{
    deco_transit_policy policy = {fixed ? DECO_TRANSIT_FIXED_GF : DECO_TRANSIT_ANCHOR_LINE,
                                  0.30, anchor};
    deco_transit_result transit = deco_transit_evaluate(staged, current,
        destination, surface, &policy, budget->points - r->inspected_points,
        &workspace->transit);
    r->inspected_points += transit.inspected_points;
    return transit.status;
}
/* All candidate state lives locally until every calculation succeeds. */
deco_anchor_result deco_anchor_transition(const deco_tissues_state *staged,
    double current, double surface, uint64_t duration,
    const deco_anchor_state *prior, const deco_anchor_allowance *allowance,
    deco_anchor_workspace *workspace)
{
    const double step = 1025.0 * 9.80665 * 3.0 / 100000.0;
    deco_anchor_allowance budget = {DECO_ANCHOR_MAX_CANDIDATES, DECO_ANCHOR_MAX_POINTS};
    deco_anchor_state proposed;
    deco_anchor_result r;
    deco_anchor_status status = DECO_ANCHOR_OK;
    deco_transit_status transit;
    deco_ceiling_result ceiling;
    deco_gf_result gf;
    deco_violation_result violation;
    deco_anchor_change change = DECO_ANCHOR_UNCHANGED;
    memset(&r, 0, sizeof(r));
    r.status = DECO_ANCHOR_ARGUMENT_ERROR;
    r.candidate.pressure_bar_abs = r.local_ceiling_bar_abs = r.gf = NAN;
    if (!staged || !prior || !workspace || !duration ||
        !deco_valid_pressure(surface) || !deco_valid_pressure(current) || current < surface ||
        overlaps(workspace, sizeof(*workspace), staged, sizeof(*staged)) ||
        overlaps(workspace, sizeof(*workspace), prior, sizeof(*prior)) ||
        (allowance && overlaps(workspace, sizeof(*workspace), allowance, sizeof(*allowance))))
        return r;
    if (allowance) budget = *allowance;
    if (budget.candidates > DECO_ANCHOR_MAX_CANDIDATES ||
        budget.points > DECO_ANCHOR_MAX_POINTS) return r;
    if (!deco_valid_state(staged)) { r.status = DECO_ANCHOR_STATE_ERROR; return r; }
    if ((!prior->present && prior->surface_dwell_ms) ||
        (prior->present && (!isfinite(prior->pressure_bar_abs) ||
         prior->pressure_bar_abs <= surface || prior->surface_dwell_ms >= DECO_ANCHOR_RESET_MS))) {
        r.status = DECO_ANCHOR_STATE_ERROR; return r;
    }
    proposed = *prior;
    if (!proposed.present) proposed.pressure_bar_abs = 0.0;
    if (!prior->present) {
        transit = query(staged, current, surface, surface, NULL, 0, &budget, workspace, &r);
        if (transit != DECO_TRANSIT_PASS && transit != DECO_TRANSIT_REJECTED) {
            r.status = mapped(transit); return r;
        }
        if (transit == DECO_TRANSIT_REJECTED) {
            unsigned n;
            status = DECO_ANCHOR_NO_VALID_ASCENT;
            /* Enumeration is bounded before any conversion, including huge pressure inputs. */
            for (n = 1; ; ++n) {
                double grid = surface + 1025.0 * 9.80665 * (3.0 * n) / 100000.0;
                if (!isfinite(grid) || grid <= surface) {
                    r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r;
                }
                if (grid > current + tolerance) break;
                if (r.attempted_candidates == budget.candidates) {
                    r.status = DECO_ANCHOR_LIMIT; return r;
                }
                ++r.attempted_candidates;
                transit = query(staged, current, fmin(grid, current), surface, NULL, 1,
                                &budget, workspace, &r);
                if (transit == DECO_TRANSIT_PASS) {
                    proposed.present = true; proposed.pressure_bar_abs = grid;
                    proposed.surface_dwell_ms = 0; change = DECO_ANCHOR_ESTABLISHED;
                    status = DECO_ANCHOR_OK; break;
                }
                if (transit != DECO_TRANSIT_REJECTED) { r.status = mapped(transit); return r; }
            }
        }
    } else {
        ceiling = deco_ceiling_fixed(staged, surface, 0.30);
        if (ceiling.status != DECO_TISSUES_OK) { r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r; }
        if (ceiling.local_ceiling_bar_abs > proposed.pressure_bar_abs + tolerance) {
            double n = fmax(1.0, ceil((ceiling.local_ceiling_bar_abs - surface - tolerance) / step));
            double grid;
            if (!isfinite(n) || n >= 9007199254740991.0) { r.status = DECO_ANCHOR_LIMIT; return r; }
            grid = surface + 1025.0 * 9.80665 * (3.0 * n) / 100000.0;
            /* Correct cancellation/rounding at the tolerance boundary using canonical pressures. */
            if (grid + tolerance < ceiling.local_ceiling_bar_abs) {
                n += 1.0;
                grid = surface + 1025.0 * 9.80665 * (3.0 * n) / 100000.0;
            }
            if (n > 1.0) {
                double previous = surface + 1025.0 * 9.80665 * (3.0 * (n - 1.0)) / 100000.0;
                if (previous + tolerance >= ceiling.local_ceiling_bar_abs) grid = previous;
            }
            if (!isfinite(grid) || grid <= surface || grid + tolerance < ceiling.local_ceiling_bar_abs) {
                r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r;
            }
            if (r.attempted_candidates == budget.candidates) { r.status = DECO_ANCHOR_LIMIT; return r; }
            ++r.attempted_candidates;
            if (grid > current + tolerance) {
                proposed.surface_dwell_ms = 0;
                status = DECO_ANCHOR_NO_VALID_ASCENT;
            } else {
                proposed.pressure_bar_abs = grid;
                change = DECO_ANCHOR_DEEPENED;
            }
        }
    }
    if (proposed.present && status != DECO_ANCHOR_NO_VALID_ASCENT) {
        if (current <= surface + 0.03 + tolerance) {
            transit = query(staged, current, surface, surface, &proposed.pressure_bar_abs, 0,
                            &budget, workspace, &r);
            if (transit != DECO_TRANSIT_PASS && transit != DECO_TRANSIT_REJECTED) {
                r.status = mapped(transit); return r;
            }
            if (transit == DECO_TRANSIT_PASS) {
                if (duration >= DECO_ANCHOR_RESET_MS - proposed.surface_dwell_ms) {
                    proposed.present = false; proposed.pressure_bar_abs = 0.0;
                    proposed.surface_dwell_ms = 0; change = DECO_ANCHOR_RESET;
                } else proposed.surface_dwell_ms += duration;
            } else proposed.surface_dwell_ms = 0;
        } else proposed.surface_dwell_ms = 0;
    }
    gf = deco_ceiling_select_gf(current, surface, proposed.present ? &proposed.pressure_bar_abs : NULL);
    if (gf.status != DECO_TISSUES_OK) { r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r; }
    ceiling = deco_ceiling_fixed(staged, surface, gf.gf);
    if (ceiling.status != DECO_TISSUES_OK) { r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r; }
    violation = deco_ceiling_check_violation(&ceiling, current);
    if (violation.status != DECO_TISSUES_OK) { r.status = DECO_ANCHOR_NUMERICAL_ERROR; return r; }
    r.status = status; r.candidate_valid = r.ceiling_valid = true;
    r.candidate = proposed; r.change = change; r.gf = gf.gf;
    r.local_ceiling_bar_abs = ceiling.local_ceiling_bar_abs; r.violated = violation.violated;
    return r;
}
