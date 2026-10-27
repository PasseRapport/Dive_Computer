#ifndef DECO_TRANSIT_H
#define DECO_TRANSIT_H
#include "deco_ceiling.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DECO_TRANSIT_PASS = 0, DECO_TRANSIT_REJECTED,
    DECO_TRANSIT_ARGUMENT_ERROR, DECO_TRANSIT_STATE_ERROR,
    DECO_TRANSIT_NUMERICAL_ERROR, DECO_TRANSIT_LIMIT
} deco_transit_status;
typedef enum { DECO_TRANSIT_FIXED_GF = 0, DECO_TRANSIT_ANCHOR_LINE } deco_transit_mode;
typedef struct {
    deco_transit_mode mode;
    double fixed_gf; /* Used only in fixed mode. */
    const double *retained_anchor_bar_abs; /* Used only in line mode; NULL absent. */
} deco_transit_policy;
typedef struct { deco_tissues_state tissues; } deco_transit_workspace;
typedef struct {
    deco_transit_status status;
    uint64_t inspected_points;
    double arrival_seconds;
    bool diagnostic_valid; /* Only first ceiling rejection. */
    double failure_seconds, failure_pressure_bar_abs, failure_gf;
    double failure_ceiling_bar_abs, failure_margin_bar;
    bool destination_valid; /* Only PASS; otherwise destination is poisoned. */
    deco_tissues_state destination;
} deco_transit_result;

/* Pure hypothetical 9 m/min ascent (1025 kg/m3, 9.80665 m/s2).
 * Checks start, integer seconds strictly before arrival, exact destination once.
 * Equal endpoints check once without exposure. Work counts inspected points.
 * Preflight LIMIT checks nothing if ceil(arrival)+1 exceeds allowance, or the
 * grid exceeds exact binary64 integer indexing (arrival >= 2^53-1 seconds).
 * No continuous-time safety proof and no stop search or schedule availability.
 * Workspace must not overlap captured state, policy, or retained anchor; such
 * overlap is an argument error. Captured state and anchor stay bytewise intact.
 * Always check status and validity tags. Margin = pressure - local ceiling.
 * Error/limit has no diagnostic or completed tissues; REJECTED has diagnostic
 * but no completed tissues. Workspace is scratch, never a completed result. */
deco_transit_result deco_transit_evaluate(const deco_tissues_state *captured,
    double start_bar_abs, double destination_bar_abs, double surface_bar_abs,
    const deco_transit_policy *policy, uint64_t allowance,
    deco_transit_workspace *workspace);
#ifdef __cplusplus
}
#endif
#endif
