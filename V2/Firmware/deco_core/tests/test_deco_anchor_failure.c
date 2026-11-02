#include "deco_anchor.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
double __real_exp(double);
double __real_expm1(double);
static unsigned calls, fail_at;
static int fail_expm1;
static unsigned ceiling_calls, fail_ceiling;
deco_ceiling_result __real_deco_ceiling_fixed(const deco_tissues_state *,double,double);
deco_ceiling_result __wrap_deco_ceiling_fixed(const deco_tissues_state *s,double surface,double gf) {
    deco_ceiling_result r=__real_deco_ceiling_fixed(s,surface,gf);
    if(++ceiling_calls==fail_ceiling)r.status=DECO_TISSUES_NUMERICAL_ERROR;
    return r;
}
double __wrap_exp(double x) { if(!fail_expm1&&++calls==fail_at)return NAN;return __real_exp(x); }
double __wrap_expm1(double x) { if(fail_expm1&&++calls==fail_at)return NAN;return __real_expm1(x); }
int main(void)
{
    deco_tissues_state s,before;
    deco_anchor_state a={true,1.5,59000},saved;
    deco_anchor_workspace w;
    unsigned segment,compartment; int op;
    const double rate=1025.0*9.80665*9.0/100000.0/60.0;
    if(deco_tissues_init_fresh(&s,1)!=DECO_TISSUES_OK)return 1;
    memcpy(&before,&s,sizeof(s));memcpy(&saved,&a,sizeof(a));
    for(op=0;op<2;op++)for(segment=0;segment<2;segment++)for(compartment=1;compartment<=16;compartment++) {
        deco_anchor_result r;
        calls=0;fail_at=16*segment+compartment;fail_expm1=op;
        r=deco_anchor_transition(&s,1+rate*1.5,1,1000,&a,NULL,&w);
        if(r.status!=DECO_ANCHOR_NUMERICAL_ERROR||r.candidate_valid||r.ceiling_valid||
           r.inspected_points!=segment+1||!isnan(r.candidate.pressure_bar_abs)||
           !isnan(r.gf)||!isnan(r.local_ceiling_bar_abs)||memcmp(&before,&s,sizeof(s))||
           memcmp(&saved,&a,sizeof(a)))return 1;
    }
    /* A final local ceiling failure must discard a provisional surface reset. */
    {
        deco_anchor_result r;
        calls=0;fail_at=0;ceiling_calls=0;fail_ceiling=3;
        r=deco_anchor_transition(&s,1,1,1000,&a,NULL,&w);
        if(r.status!=DECO_ANCHOR_NUMERICAL_ERROR||r.candidate_valid||r.ceiling_valid||
           r.change!=DECO_ANCHOR_UNCHANGED||!isnan(r.candidate.pressure_bar_abs)||
           !isnan(r.local_ceiling_bar_abs)||memcmp(&before,&s,sizeof(s))||
           memcmp(&saved,&a,sizeof(a)))return 1;
    }
    puts("64 compartment/segment anchor arithmetic faults preserve captured inputs");return 0;
}
