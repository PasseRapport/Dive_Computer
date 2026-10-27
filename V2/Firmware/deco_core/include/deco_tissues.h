#ifndef DECO_TISSUES_H
#define DECO_TISSUES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DECO_TISSUE_COUNT 16
#define DECO_POLICY_ID_CAPACITY 48
#define DECO_COEFFICIENT_HASH_CAPACITY 65

typedef enum {
    DECO_TISSUES_OK = 0,
    DECO_TISSUES_ARGUMENT_ERROR,
    DECO_TISSUES_STATE_ERROR,
    DECO_TISSUES_NUMERICAL_ERROR
} deco_tissues_result;

typedef struct {
    char policy_id[DECO_POLICY_ID_CAPACITY];
    char coefficient_sha256[DECO_COEFFICIENT_HASH_CAPACITY];
    double nitrogen_bar[DECO_TISSUE_COUNT];
} deco_tissues_state;

typedef struct {
    double half_time_min;
    double a_bar;
    double b;
} deco_nitrogen_coefficient;

typedef struct {
    const char *policy_id;
    const char *coefficient_id;
    const char *coefficient_sha256;
    double nitrogen_fraction;
    double water_vapour_bar;
    deco_nitrogen_coefficient compartments[DECO_TISSUE_COUNT];
} deco_tissues_constants;

/* Immutable identity and accepted 4-minute ZH-L16C nitrogen table. */
const deco_tissues_constants *deco_tissues_get_constants(void);

/* Explicitly replace state with a fresh synthetic equilibrium. No history claim.
 * Absolute pressures must be finite and >= water_vapour_bar (zero inspired N2
 * is mathematically valid). No depth, session, timestamp or gap policy applies.
 * All errors leave every byte of the supplied state unchanged. */
deco_tissues_result deco_tissues_init_fresh(deco_tissues_state *state,
                                           double surface_bar_abs);

/* Integrate a linear absolute ambient-pressure segment. Equal endpoints are a
 * hold, including uptake/elimination from nonequilibrium tissues. Duration is
 * positive measured milliseconds, converted to binary64 minutes; very large
 * uint64 values are rounded to binary64. State must have matching identities
 * and 16 finite nonnegative nitrogen pressures. Never initializes implicitly.
 * All candidate outputs are checked before committing; errors are atomic. */
deco_tissues_result deco_tissues_integrate(deco_tissues_state *state,
                                          double start_bar_abs,
                                          double end_bar_abs,
                                          uint64_t duration_ms);

/* Hypothetical linear segment, positive finite binary64 seconds, converted
 * directly to minutes. Same validation and atomic rejection as measured API.
 * Extremely small positive durations may produce a numerical error on underflow.
 * Mutates only the supplied prediction state; callers must copy live exposure. */
deco_tissues_result deco_tissues_predict_seconds(deco_tissues_state *state,
                                                double start_bar_abs,
                                                double end_bar_abs,
                                                double duration_seconds);

#ifdef __cplusplus
}
#endif
#endif
