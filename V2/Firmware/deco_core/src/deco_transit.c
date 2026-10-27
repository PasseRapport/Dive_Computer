#include "deco_transit.h"
#include "deco_validation.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int overlap(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}
static deco_transit_status mapped(deco_tissues_result status)
{
    if (status == DECO_TISSUES_ARGUMENT_ERROR) return DECO_TRANSIT_ARGUMENT_ERROR;
    if (status == DECO_TISSUES_STATE_ERROR) return DECO_TRANSIT_STATE_ERROR;
    return DECO_TRANSIT_NUMERICAL_ERROR;
}

deco_transit_result deco_transit_evaluate(const deco_tissues_state *captured,
    double start_bar_abs, double destination_bar_abs, double surface_bar_abs,
    const deco_transit_policy *policy, uint64_t allowance,
    deco_transit_workspace *workspace)
{
    const double rate = 1025.0 * 9.80665 * 9.0 / 100000.0 / 60.0;
    deco_transit_result r;
    double previous_time = 0.0, previous_pressure = start_bar_abs;
    double duration;
    uint64_t required, index;
    size_t i;
    memset(&r, 0, sizeof(r));
    r.status = DECO_TRANSIT_ARGUMENT_ERROR;
    r.arrival_seconds = r.failure_seconds = r.failure_pressure_bar_abs = NAN;
    r.failure_gf = r.failure_ceiling_bar_abs = r.failure_margin_bar = NAN;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) r.destination.nitrogen_bar[i] = NAN;
    if (!captured || !policy || !workspace ||
        !deco_valid_pressure(surface_bar_abs) ||
        !deco_valid_pressure(start_bar_abs) ||
        !deco_valid_pressure(destination_bar_abs) ||
        start_bar_abs < destination_bar_abs || destination_bar_abs < surface_bar_abs ||
        overlap(workspace, sizeof(*workspace), captured, sizeof(*captured)) ||
        overlap(workspace, sizeof(*workspace), policy, sizeof(*policy))) return r;
    if (policy->mode == DECO_TRANSIT_FIXED_GF) {
        if (!isfinite(policy->fixed_gf) || policy->fixed_gf <= 0.0 ||
            policy->fixed_gf > 1.0) return r;
    } else if (policy->mode == DECO_TRANSIT_ANCHOR_LINE) {
        if (policy->retained_anchor_bar_abs && overlap(workspace, sizeof(*workspace),
            policy->retained_anchor_bar_abs, sizeof(double))) return r;
        if (deco_ceiling_select_gf(start_bar_abs, surface_bar_abs,
            policy->retained_anchor_bar_abs).status != DECO_TISSUES_OK) return r;
    } else return r;
    if (!deco_valid_state(captured)) { r.status = DECO_TRANSIT_STATE_ERROR; return r; }
    duration = (start_bar_abs - destination_bar_abs) / rate;
    if (!isfinite(duration)) { r.status = DECO_TRANSIT_NUMERICAL_ERROR; return r; }
    r.arrival_seconds = duration;
    if (duration >= 9007199254740991.0) { r.status = DECO_TRANSIT_LIMIT; return r; }
    required = duration == 0.0 ? 1 : (uint64_t)ceil(duration) + 1;
    if (required > allowance) { r.status = DECO_TRANSIT_LIMIT; return r; }
    memcpy(&workspace->tissues, captured, sizeof(*captured));
    for (index = 0; index < required; ++index) {
        double time = index == required - 1 ? duration : (double)index;
        double pressure = index == required - 1 ? destination_bar_abs :
                          start_bar_abs - rate * time;
        double gf = policy->mode == DECO_TRANSIT_FIXED_GF ? policy->fixed_gf : NAN;
        deco_ceiling_result ceiling;
        deco_violation_result violation;
        if (index != 0) {
            deco_tissues_result status = deco_tissues_predict_seconds(&workspace->tissues,
                previous_pressure, pressure, time - previous_time);
            if (status != DECO_TISSUES_OK) { r.status = mapped(status); return r; }
        }
        if (policy->mode == DECO_TRANSIT_ANCHOR_LINE) {
            deco_gf_result selected = deco_ceiling_select_gf(pressure, surface_bar_abs,
                                                           policy->retained_anchor_bar_abs);
            if (selected.status != DECO_TISSUES_OK) { r.status = mapped(selected.status); return r; }
            gf = selected.gf;
        }
        ceiling = deco_ceiling_fixed(&workspace->tissues, surface_bar_abs, gf);
        if (ceiling.status != DECO_TISSUES_OK) { r.status = mapped(ceiling.status); return r; }
        violation = deco_ceiling_check_violation(&ceiling, pressure);
        if (violation.status != DECO_TISSUES_OK) { r.status = mapped(violation.status); return r; }
        ++r.inspected_points;
        if (violation.violated) {
            r.status = DECO_TRANSIT_REJECTED; r.diagnostic_valid = true;
            r.failure_seconds = time; r.failure_pressure_bar_abs = pressure;
            r.failure_gf = gf; r.failure_ceiling_bar_abs = ceiling.local_ceiling_bar_abs;
            r.failure_margin_bar = pressure - ceiling.local_ceiling_bar_abs;
            return r;
        }
        previous_time = time; previous_pressure = pressure;
    }
    r.status = DECO_TRANSIT_PASS; r.destination_valid = true;
    memcpy(&r.destination, &workspace->tissues, sizeof(r.destination));
    return r;
}
