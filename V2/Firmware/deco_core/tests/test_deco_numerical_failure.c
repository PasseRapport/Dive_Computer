/* Host fault injection verifies late-compartment libm failures without adding
 * fault controls or mutable globals to the production library. */
#include "deco_tissues.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

double __real_exp(double value);
double __real_expm1(double value);
static unsigned calls;
static unsigned fail_at;
static int fail_expm1;

double __wrap_exp(double value)
{
    if (!fail_expm1 && ++calls == fail_at) return NAN;
    return __real_exp(value);
}

double __wrap_expm1(double value)
{
    if (fail_expm1 && ++calls == fail_at) return NAN;
    return __real_expm1(value);
}

int main(void)
{
    deco_tissues_state state;
    unsigned char before[sizeof(state)];
    unsigned compartment;
    int operation;
    for (operation = 0; operation < 2; ++operation) {
        for (compartment = 1; compartment <= DECO_TISSUE_COUNT; ++compartment) {
            if (deco_tissues_init_fresh(&state, 1.01325) != DECO_TISSUES_OK) return 1;
            memcpy(before, &state, sizeof(state));
            calls = 0;
            fail_at = compartment;
            fail_expm1 = operation;
            if (deco_tissues_integrate(&state, 1.01325, 4, 123456) !=
                    DECO_TISSUES_NUMERICAL_ERROR ||
                memcmp(before, &state, sizeof(state)) != 0) {
                fprintf(stderr, "Nonatomic numerical rejection at compartment %u operation %d\n",
                        compartment, operation);
                return 1;
            }
        }
    }
    puts("All 32 injected numerical failures rejected without changing state");
    return 0;
}
