/* Link-time fault injection stays outside the portable production code. */
#include "deco_transit.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
double __real_exp(double);
double __real_expm1(double);
static unsigned calls, fail_at;
static int fail_expm1;
double __wrap_exp(double x) { if(!fail_expm1 && ++calls==fail_at)return NAN;return __real_exp(x); }
double __wrap_expm1(double x) { if(fail_expm1 && ++calls==fail_at)return NAN;return __real_expm1(x); }
int main(void)
{
    deco_tissues_state s,before;
    deco_transit_workspace w;
    double anchor=1.5,before_anchor;
    deco_transit_policy p={DECO_TRANSIT_ANCHOR_LINE,0,&anchor};
    unsigned compartment, segment;int op;
    const double rate=1025.0*9.80665*9.0/100000.0/60.0;
    if(deco_tissues_init_fresh(&s,1)!=DECO_TISSUES_OK)return 1;
    memcpy(&before,&s,sizeof(s));memcpy(&before_anchor,&anchor,sizeof(anchor));
    /* 2.5-second path: segment 0 fails before any successful integration,
     * segment 1 after a regular point, segment 2 in the fractional arrival.
     * Exercise every compartment of each libm operation at each stage. */
    for(op=0;op<2;op++)for(segment=0;segment<3;segment++)
    for(compartment=1;compartment<=16;compartment++) {
        deco_transit_result r;
        size_t i;
        calls=0;fail_at=16*segment+compartment;fail_expm1=op;
        r=deco_transit_evaluate(&s,2,2-rate*2.5,1,&p,4,&w);
        if(r.status!=DECO_TRANSIT_NUMERICAL_ERROR || r.destination_valid ||
           r.diagnostic_valid || r.inspected_points!=segment+1 ||
           !isnan(r.failure_seconds) || !isnan(r.failure_pressure_bar_abs) ||
           !isnan(r.failure_gf) || !isnan(r.failure_ceiling_bar_abs) ||
           !isnan(r.failure_margin_bar) ||
           memcmp(&before,&s,sizeof(s)) || memcmp(&anchor,&before_anchor,sizeof(anchor)))return 1;
        for(i=0;i<DECO_TISSUE_COUNT;i++)if(!isnan(r.destination.nitrogen_bar[i]))return 1;
    }
    /* Direct prediction must remain atomic even after earlier compartments
     * succeeded; this public entry point shares no test-only fault switches. */
    for(op=0;op<2;op++)for(compartment=1;compartment<=16;compartment++) {
        calls=0;fail_at=compartment;fail_expm1=op;
        if(deco_tissues_predict_seconds(&s,2,1,2.5)!=DECO_TISSUES_NUMERICAL_ERROR ||
           memcmp(&before,&s,sizeof(s)))return 1;
    }
    puts("96 staged transit faults and 32 direct prediction faults preserve inputs");return 0;
}
