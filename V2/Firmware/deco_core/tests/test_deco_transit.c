#include "deco_transit.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
/* Union members provide real, correctly aligned typed objects. The evaluator
 * only takes the address of the overlapping workspace before rejecting it.
 * Anchor containment is tested at both workspace boundaries: a double cannot
 * straddle an aligned workspace boundary on this binary64 ABI. */
static int overlap_contracts(void)
{
    union fixture {
        deco_transit_workspace workspace;
        deco_tissues_state captured;
        deco_transit_policy policy;
        double anchor;
        struct { double prefix; deco_tissues_state captured; } shifted_state;
        struct {
            unsigned char prefix[sizeof(deco_transit_workspace) - sizeof(double)];
            deco_transit_policy policy;
        } shifted_policy;
        struct {
            unsigned char prefix[sizeof(deco_transit_workspace) - sizeof(double)];
            double anchor;
        } end_anchor;
    } fixture;
    unsigned char bytes[sizeof(fixture)];
    deco_tissues_state state, before;
    unsigned kind;
    CHECK(deco_tissues_init_fresh(&state, 1) == DECO_TISSUES_OK);
    memcpy(&before, &state, sizeof(state));
    for (kind = 0; kind < 6; ++kind) {
        deco_transit_policy policy = {DECO_TRANSIT_FIXED_GF, 0.7, NULL};
        const deco_transit_policy *supplied_policy = &policy;
        const deco_tissues_state *captured = &state;
        deco_transit_result result;
        unsigned char policy_bytes[sizeof(policy)];
        memset(&fixture, 0, sizeof(fixture));
        switch (kind) {
        case 0:
            memcpy(&fixture.captured, &state, sizeof(state));
            captured = &fixture.captured;
            break;
        case 1:
            memcpy(&fixture.shifted_state.captured, &state, sizeof(state));
            captured = &fixture.shifted_state.captured;
            break;
        case 2:
            fixture.policy = policy; supplied_policy = &fixture.policy;
            break;
        case 3:
            fixture.shifted_policy.policy = policy;
            supplied_policy = &fixture.shifted_policy.policy;
            break;
        case 4:
            fixture.anchor = 1.5;
            policy.mode = DECO_TRANSIT_ANCHOR_LINE;
            policy.retained_anchor_bar_abs = &fixture.anchor;
            break;
        default:
            fixture.end_anchor.anchor = 1.5;
            policy.mode = DECO_TRANSIT_ANCHOR_LINE;
            policy.retained_anchor_bar_abs = &fixture.end_anchor.anchor;
            break;
        }
        memcpy(bytes, &fixture, sizeof(fixture));
        memcpy(policy_bytes, &policy, sizeof(policy));
        result = deco_transit_evaluate(captured, 2, 1, 1, supplied_policy,
                                      100, &fixture.workspace);
        CHECK(result.status == DECO_TRANSIT_ARGUMENT_ERROR);
        CHECK(result.inspected_points == 0 && !result.destination_valid &&
              !result.diagnostic_valid);
        CHECK(isnan(result.failure_seconds) && isnan(result.failure_pressure_bar_abs) &&
              isnan(result.failure_gf) && isnan(result.failure_ceiling_bar_abs) &&
              isnan(result.failure_margin_bar));
        { size_t i; for (i = 0; i < DECO_TISSUE_COUNT; ++i)
            CHECK(isnan(result.destination.nitrogen_bar[i])); }
        CHECK(memcmp(bytes, &fixture, sizeof(fixture)) == 0);
        CHECK(memcmp(policy_bytes, &policy, sizeof(policy)) == 0);
        CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    }
    return 0;
}

static int prediction_errors(void)
{
    const double invalid[] = {0.0, -1.0, NAN, INFINITY, -INFINITY};
    deco_tissues_state state, before;
    size_t i;
    CHECK(deco_tissues_init_fresh(&state, 1) == DECO_TISSUES_OK);
    memcpy(&before, &state, sizeof(state));
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        CHECK(deco_tissues_predict_seconds(&state, 2, 1, invalid[i]) ==
              DECO_TISSUES_ARGUMENT_ERROR);
        CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    }
    CHECK(deco_tissues_predict_seconds(&state, 2, 1, nextafter(0.0, 1.0)) ==
          DECO_TISSUES_NUMERICAL_ERROR);
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    return 0;
}

int main(void)
{
    CHECK(overlap_contracts() == 0);
    CHECK(prediction_errors() == 0);
    const double rate=1025.0*9.80665*9.0/100000.0/60.0;
    deco_tissues_state s,before,expected;
    deco_transit_workspace w;
    double anchor=1.5,anchor_before;
    deco_transit_policy p={DECO_TRANSIT_FIXED_GF,0.7,NULL};
    deco_transit_result r,r2;
    size_t i;
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);
    memcpy(&before,&s,sizeof(s));
    r=deco_transit_evaluate(&s,2,2,1,&p,1,&w);
    CHECK(r.status==DECO_TRANSIT_PASS && r.inspected_points==1 && r.arrival_seconds==0);
    CHECK(r.destination_valid && !r.diagnostic_valid && !memcmp(&s,&r.destination,sizeof(s)));
    r=deco_transit_evaluate(&s,2,2,1,&p,0,&w);
    CHECK(r.status==DECO_TRANSIT_LIMIT && !r.destination_valid && !r.inspected_points);
    r=deco_transit_evaluate(&s,2,2-rate*2.5,1,&p,4,&w);
    CHECK(r.status==DECO_TRANSIT_PASS && r.inspected_points==4);
    CHECK(r.arrival_seconds==(2-(2-rate*2.5))/rate);
    memcpy(&expected,&s,sizeof(s));
    CHECK(deco_tissues_predict_seconds(&expected,2,2-rate*2.5,r.arrival_seconds)==DECO_TISSUES_OK);
    for(i=0;i<16;i++)CHECK(fabs(expected.nitrogen_bar[i]-r.destination.nitrogen_bar[i])<1e-14);
    r2=deco_transit_evaluate(&s,2,2-rate*2.5,1,&p,4,&w);
    CHECK(!memcmp(&r,&r2,sizeof(r)));
    r=deco_transit_evaluate(&s,2,2-rate*2.5,1,&p,3,&w);
    CHECK(r.status==DECO_TRANSIT_LIMIT && !r.inspected_points && !r.destination_valid);
    p.mode=DECO_TRANSIT_ANCHOR_LINE;p.retained_anchor_bar_abs=&anchor;
    memcpy(&anchor_before,&anchor,sizeof(anchor));
    r=deco_transit_evaluate(&s,2,1,1,&p,100,&w);
    CHECK(r.status==DECO_TRANSIT_PASS && r.inspected_points==(uint64_t)ceil(r.arrival_seconds)+1);
    CHECK(!memcmp(&anchor,&anchor_before,sizeof(anchor)) && !memcmp(&s,&before,sizeof(s)));
    p.mode=DECO_TRANSIT_FIXED_GF;p.fixed_gf=0.3;
    for(i=0;i<16;i++)s.nitrogen_bar[i]=3;
    memcpy(&before,&s,sizeof(s));
    r=deco_transit_evaluate(&s,1,1,1,&p,1,&w);
    CHECK(r.status==DECO_TRANSIT_REJECTED && r.diagnostic_valid && r.failure_seconds==0);
    CHECK(!r.destination_valid && r.failure_margin_bar< -1e-9);
    CHECK(!memcmp(&s,&before,sizeof(s)));
    r=deco_transit_evaluate(&s,3,1,1,&p,1000,&w);
    CHECK(r.status==DECO_TRANSIT_REJECTED && r.failure_seconds>0 && r.failure_seconds<r.arrival_seconds);
    CHECK(r.inspected_points==(uint64_t)r.failure_seconds+1);
    /* Engineer an endpoint failure after a passing start, subsecond arrival. */
    for(i=0;i<16;i++)s.nitrogen_bar[i]=0;
    s.nitrogen_bar[15]=1.2;
    { deco_ceiling_result c=deco_ceiling_fixed(&s,1,0.3);
      r=deco_transit_evaluate(&s,c.local_ceiling_bar_abs+rate*0.1,
                            c.local_ceiling_bar_abs-rate*0.1,1,&p,2,&w);
      CHECK(r.status==DECO_TRANSIT_REJECTED && r.inspected_points==2);
      CHECK(r.failure_seconds==r.arrival_seconds); }
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);memcpy(&before,&s,sizeof(s));
    p.fixed_gf=NAN;r=deco_transit_evaluate(&s,2,1,1,&p,1000,&w);
    CHECK(r.status==DECO_TRANSIT_ARGUMENT_ERROR && !r.destination_valid && isnan(r.destination.nitrogen_bar[0]));
    p.fixed_gf=.7;s.policy_id[0]^=1;
    r=deco_transit_evaluate(&s,2,1,1,&p,1000,&w);CHECK(r.status==DECO_TRANSIT_STATE_ERROR);
    memcpy(&s,&before,sizeof(s));
    r=deco_transit_evaluate(&s,DBL_MAX,1,1,&p,UINT64_MAX,&w);CHECK(r.status==DECO_TRANSIT_NUMERICAL_ERROR);
    r=deco_transit_evaluate(&s,1e20,1,1,&p,UINT64_MAX,&w);CHECK(r.status==DECO_TRANSIT_LIMIT && !r.inspected_points);
    r=deco_transit_evaluate(&s,2,1,1,&p,1000,(deco_transit_workspace*)&s);
    CHECK(r.status==DECO_TRANSIT_ARGUMENT_ERROR && !memcmp(&s,&before,sizeof(s)));
    r=deco_transit_evaluate(NULL,2,1,1,&p,1000,&w);CHECK(r.status==DECO_TRANSIT_ARGUMENT_ERROR);
    r=deco_transit_evaluate(&s,1,2,1,&p,1000,&w);CHECK(r.status==DECO_TRANSIT_ARGUMENT_ERROR);
    r=deco_transit_evaluate(&s,2,NAN,1,&p,1000,&w);CHECK(r.status==DECO_TRANSIT_ARGUMENT_ERROR);
    puts("Transit grid, diagnostics, bounds and purity contracts passed");return 0;
}
