#include "deco_anchor.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static double grid(unsigned n) { return 1.0+1025.0*9.80665*(3.0*n)/100000.0; }
static deco_anchor_result run(const deco_tissues_state *s,double current,uint64_t ms,
    const deco_anchor_state *a,const deco_anchor_allowance *b) {
    deco_anchor_workspace w;
    return deco_anchor_transition(s,current,1,ms,a,b,&w);
}
static void uniform(deco_tissues_state *s,double nitrogen) {
    unsigned i;for(i=0;i<16;i++)s->nitrogen_bar[i]=nitrogen;
}
int main(void)
{
    deco_tissues_state s,before;
    deco_anchor_state a={0},saved;
    deco_anchor_allowance b={20,9000};
    deco_anchor_result r,t;
    deco_anchor_workspace w;
    uint64_t inspected;
    unsigned i;
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);
    memcpy(&before,&s,sizeof(s));memcpy(&saved,&a,sizeof(a));
    r=run(&s,4,1000,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_OK && r.candidate_valid && r.ceiling_valid);
    CHECK(!r.candidate.present && !r.candidate.surface_dwell_ms && !r.attempted_candidates);
    CHECK(!memcmp(&before,&s,sizeof(s))&&!memcmp(&saved,&a,sizeof(a)));
    inspected=r.inspected_points;b.points=inspected;
    CHECK(run(&s,4,1000,&a,&b).status==DECO_ANCHOR_OK);
    b.points=inspected-1;r=run(&s,4,1000,&a,&b);
    CHECK(r.status==DECO_ANCHOR_LIMIT&&!r.candidate_valid&&!r.ceiling_valid&&!r.inspected_points);
    CHECK(isnan(r.gf)&&isnan(r.local_ceiling_bar_abs)&&isnan(r.candidate.pressure_bar_abs));
    b.points=9000;
    /* Canonical grid anchor survives a tolerance-eligible zero-distance query. */
    uniform(&s,1.2);r=run(&s,grid(1)-0.5e-9,1000,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_OK&&r.candidate.pressure_bar_abs==grid(1)&&r.attempted_candidates==1);
    r=run(&s,grid(1)-2e-9,1000,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_NO_VALID_ASCENT&&!r.candidate.present&&!r.attempted_candidates);
    /* Search fixture for genuinely required, shallowest GF-low destination. */
    uniform(&s,1.6);r=run(&s,4,1000,&a,&b);
    CHECK(r.status==DECO_ANCHOR_OK&&r.candidate.present&&r.change==DECO_ANCHOR_ESTABLISHED);
    CHECK(r.attempted_candidates>=1&&r.candidate.pressure_bar_abs==grid((unsigned)r.attempted_candidates));
    b.candidates=r.attempted_candidates-1;t=run(&s,4,1000,&a,&b);
    CHECK(t.status==DECO_ANCHOR_LIMIT&&!t.candidate_valid);
    b.candidates=r.attempted_candidates;t=run(&s,4,1000,&a,&b);
    CHECK(t.status==r.status&&t.candidate.pressure_bar_abs==r.candidate.pressure_bar_abs);
    b.candidates=20;
    uniform(&s,20);r=run(&s,grid(20),1000,&a,&b);
    CHECK(r.status==DECO_ANCHOR_NO_VALID_ASCENT&&r.candidate_valid&&!r.candidate.present&&r.attempted_candidates==20);
    r=run(&s,grid(21),1000,&a,&b);
    CHECK(r.status==DECO_ANCHOR_LIMIT&&r.attempted_candidates==20&&!r.candidate_valid);
    r=run(&s,1.1,1000,&a,&b);
    CHECK(r.status==DECO_ANCHOR_NO_VALID_ASCENT&&r.attempted_candidates==0&&r.violated);
    a.present=true;a.pressure_bar_abs=grid(1);a.surface_dwell_ms=123;
    uniform(&s,1.6);r=run(&s,4,1000,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_OK&&r.change==DECO_ANCHOR_DEEPENED&&r.attempted_candidates==1);
    CHECK(r.candidate.pressure_bar_abs>a.pressure_bar_abs&&r.candidate.surface_dwell_ms==0);
    t=run(&s,1.1,1000,&a,NULL);
    CHECK(t.status==DECO_ANCHOR_NO_VALID_ASCENT&&t.candidate.pressure_bar_abs==a.pressure_bar_abs&&!t.candidate.surface_dwell_ms);
    /* Fixed-low ceiling boundary: arithmetic equality retains; only > tolerance deepens. */
    {
        const deco_tissues_constants *c=deco_tissues_get_constants();
        deco_ceiling_result low;
        uniform(&s,0);
        s.nitrogen_bar[15]=grid(1)*(0.7+0.3/c->compartments[15].b)+0.3*c->compartments[15].a_bar;
        low=deco_ceiling_fixed(&s,1,0.3);
        a.pressure_bar_abs=low.local_ceiling_bar_abs-0.5e-9;
        r=run(&s,2,1000,&a,NULL);
        CHECK(r.change==DECO_ANCHOR_UNCHANGED&&r.attempted_candidates==0);
        a.pressure_bar_abs=low.local_ceiling_bar_abs-2e-9;
        r=run(&s,2,1000,&a,NULL);
        CHECK(r.change==DECO_ANCHOR_DEEPENED&&r.candidate.pressure_bar_abs==grid(1));
        b.candidates=0;b.points=9000;
        CHECK(run(&s,2,1000,&a,&b).status==DECO_ANCHOR_LIMIT);b.candidates=20;
        s.nitrogen_bar[15]=(grid(1)+0.5e-9)*(0.7+0.3/c->compartments[15].b)+0.3*c->compartments[15].a_bar;
        a.pressure_bar_abs=grid(1)-.1;
        r=run(&s,grid(1)-0.5e-9,1000,&a,NULL);
        CHECK(r.change==DECO_ANCHOR_DEEPENED&&r.candidate.pressure_bar_abs==grid(1));
    }
    a.pressure_bar_abs=grid(10);a.surface_dwell_ms=0;
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);
    r=run(&s,grid(1),1000,&a,NULL);t=run(&s,grid(15),1000,&r.candidate,NULL);
    CHECK(r.candidate.pressure_bar_abs==a.pressure_bar_abs&&t.candidate.pressure_bar_abs==a.pressure_bar_abs);
    a.surface_dwell_ms=59000;
    r=run(&s,1,999,&a,NULL);CHECK(r.candidate.present&&r.candidate.surface_dwell_ms==59999);
    r=run(&s,1,1000,&a,NULL);CHECK(r.change==DECO_ANCHOR_RESET&&!r.candidate.present&&!r.candidate.surface_dwell_ms);
    r=run(&s,1,UINT64_MAX,&a,NULL);CHECK(r.change==DECO_ANCHOR_RESET);
    r=run(&s,1.03,999,&a,NULL);CHECK(r.candidate.surface_dwell_ms==59999);
    r=run(&s,1.03+1e-9,999,&a,NULL);CHECK(r.candidate.surface_dwell_ms==59999);
    r=run(&s,1.03+2e-9,999,&a,NULL);CHECK(r.candidate.surface_dwell_ms==0&&r.candidate.present);
    /* Local GF-high surface failure with low ceiling below the retained anchor. */
    uniform(&s,2);r=run(&s,1,1000,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_OK&&r.candidate.present&&!r.candidate.surface_dwell_ms&&r.violated);
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);
    a.surface_dwell_ms=0;
    r=run(&s,1,1000,&a,NULL);CHECK(r.candidate.surface_dwell_ms==1000);
    t=run(&s,2,1000,&r.candidate,NULL);CHECK(!t.candidate.surface_dwell_ms);
    r=run(&s,1,1000,&t.candidate,NULL);CHECK(r.candidate.surface_dwell_ms==1000);
    memcpy(&before,&s,sizeof(s));memcpy(&saved,&a,sizeof(a));
    for(i=0;i<20;i++) {
        r=run(&s,1,1000,&a,NULL);(void)run(&s,4,3000,&a,NULL);t=run(&s,1,1000,&a,NULL);
        CHECK(!memcmp(&r,&t,sizeof(r)));
    }
    CHECK(!memcmp(&before,&s,sizeof(s))&&!memcmp(&saved,&a,sizeof(a)));
    CHECK(run(&s,1,0,&a,NULL).status==DECO_ANCHOR_ARGUMENT_ERROR);
    CHECK(run(&s,.9,1,&a,NULL).status==DECO_ANCHOR_ARGUMENT_ERROR);
    CHECK(run(&s,NAN,1,&a,NULL).status==DECO_ANCHOR_ARGUMENT_ERROR);
    b.candidates=21;CHECK(run(&s,1,1,&a,&b).status==DECO_ANCHOR_ARGUMENT_ERROR);
    b.candidates=20;b.points=9001;CHECK(run(&s,1,1,&a,&b).status==DECO_ANCHOR_ARGUMENT_ERROR);
    a.surface_dwell_ms=60000;CHECK(run(&s,1,1,&a,NULL).status==DECO_ANCHOR_STATE_ERROR);
    a.surface_dwell_ms=0;a.pressure_bar_abs=1;CHECK(run(&s,1,1,&a,NULL).status==DECO_ANCHOR_STATE_ERROR);
    a.present=false;a.surface_dwell_ms=1;CHECK(run(&s,1,1,&a,NULL).status==DECO_ANCHOR_STATE_ERROR);
    a.surface_dwell_ms=0;a.pressure_bar_abs=NAN;r=run(&s,1,1,&a,NULL);
    CHECK(r.status==DECO_ANCHOR_OK&&!r.candidate.present&&r.candidate.pressure_bar_abs==0);
    s.policy_id[0]^=1;CHECK(run(&s,1,1,&a,NULL).status==DECO_ANCHOR_STATE_ERROR);s.policy_id[0]^=1;
    s.nitrogen_bar[0]=-1;CHECK(run(&s,1,1,&a,NULL).status==DECO_ANCHOR_STATE_ERROR);
    CHECK(deco_tissues_init_fresh(&s,1)==DECO_TISSUES_OK);
    /* Scratch alias rejection precedes all writes, including partial overlaps. */
    CHECK(deco_anchor_transition(&s,1,1,1,&a,NULL,(deco_anchor_workspace *)&s).status==DECO_ANCHOR_ARGUMENT_ERROR);
    memcpy(&before,&s,sizeof(s));
    CHECK(deco_anchor_transition(&s,1,1,1,(deco_anchor_state *)&w,NULL,&w).status==DECO_ANCHOR_ARGUMENT_ERROR);
    CHECK(deco_anchor_transition(&s,1,1,1,&a,(deco_anchor_allowance *)&w,&w).status==DECO_ANCHOR_ARGUMENT_ERROR);
    CHECK(!memcmp(&before,&s,sizeof(s)));
    /* Transit rejects enormous virtual index counts before integration. */
    r=run(&s,DBL_MAX,1,&a,NULL);CHECK(r.status==DECO_ANCHOR_NUMERICAL_ERROR&&!r.candidate_valid);
    r=run(&s,1e20,1,&a,NULL);CHECK(r.status==DECO_ANCHOR_LIMIT&&!r.candidate_valid);
    puts("anchor lifecycle, exact work bounds, purity and overlap contracts pass");
    return 0;
}
