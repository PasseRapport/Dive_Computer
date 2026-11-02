#ifndef DECO_ANCHOR_H
#define DECO_ANCHOR_H
#include "deco_transit.h"
#ifdef __cplusplus
extern "C" {
#endif
#define DECO_ANCHOR_MAX_CANDIDATES 20
#define DECO_ANCHOR_MAX_POINTS 9000
#define DECO_ANCHOR_RESET_MS UINT64_C(60000)
typedef struct {
    bool present;
    double pressure_bar_abs; /* Ignored when absent; output is canonical zero. */
    uint64_t surface_dwell_ms; /* Absent: zero; present: strictly below 60000. */
} deco_anchor_state;
typedef enum {
    DECO_ANCHOR_OK = 0, DECO_ANCHOR_NO_VALID_ASCENT,
    DECO_ANCHOR_ARGUMENT_ERROR, DECO_ANCHOR_STATE_ERROR,
    DECO_ANCHOR_NUMERICAL_ERROR, DECO_ANCHOR_LIMIT
} deco_anchor_status;
typedef enum {
    DECO_ANCHOR_UNCHANGED = 0, DECO_ANCHOR_ESTABLISHED,
    DECO_ANCHOR_DEEPENED, DECO_ANCHOR_RESET
} deco_anchor_change;
typedef struct { uint64_t candidates, points; } deco_anchor_allowance;
typedef struct { deco_transit_workspace transit; } deco_anchor_workspace;
typedef struct {
    deco_anchor_status status;
    bool candidate_valid, ceiling_valid, violated;
    deco_anchor_change change;
    deco_anchor_state candidate;
    double local_ceiling_bar_abs, gf;
    uint64_t attempted_candidates, inspected_points;
} deco_anchor_result;
/* Pure transition over already staged exposure. Positive accepted duration only;
 * cadence/history ownership is external. Never commits tissues or input state.
 * NULL allowance selects 20 grid attempts / 9000 actual virtual point checks.
 * Smaller allowances are permitted; larger ones are argument errors. Transit
 * retains its full-path preflight LIMIT semantics. Scratch must be disjoint
 * from every input object. Only OK and NO_VALID_ASCENT have a valid candidate
 * and local ceiling. Neither status promises a complete ascent schedule.
 * Reset after 60000 qualifying accepted ms within 0.03 + 1e-9 bar of surface,
 * passing direct transit on the retained GF line. Never shifts shallower
 * underwater. Errors poison numerical outputs and expose no candidate. */
deco_anchor_result deco_anchor_transition(const deco_tissues_state *staged,
    double current_bar_abs, double surface_bar_abs, uint64_t accepted_duration_ms,
    const deco_anchor_state *prior, const deco_anchor_allowance *allowance,
    deco_anchor_workspace *workspace);
#ifdef __cplusplus
}
#endif
#endif
