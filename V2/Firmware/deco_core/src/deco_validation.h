#ifndef DECO_VALIDATION_H
#define DECO_VALIDATION_H

#include "deco_tissues.h"
#include <math.h>
#include <string.h>

/* Private validation shared by integration and ceiling; no state mutation. */
static inline int deco_valid_pressure(double pressure)
{
    return isfinite(pressure) &&
           pressure >= deco_tissues_get_constants()->water_vapour_bar;
}

static inline int deco_valid_state(const deco_tissues_state *state)
{
    const deco_tissues_constants *c = deco_tissues_get_constants();
    size_t i;
    if (strncmp(state->policy_id, c->policy_id, sizeof(state->policy_id)) != 0 ||
        strncmp(state->coefficient_sha256, c->coefficient_sha256,
                sizeof(state->coefficient_sha256)) != 0) return 0;
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        if (!isfinite(state->nitrogen_bar[i]) || state->nitrogen_bar[i] < 0.0)
            return 0;
    }
    return 1;
}
#endif
