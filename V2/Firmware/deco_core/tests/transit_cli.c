/* Host adapter. fixed|line SURFACE START DEST GF|none|ANCHOR ALLOWANCE
 * IDENTITY P0 ... P15. IDENTITY 0 normal, 1 bad policy, 2 bad coefficient.
 * One JSON identity header, then one result per input line. Malformed input
 * exits 2; validly parsed physiological errors return tagged JSON. */
#include "deco_transit.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int num(const char *s, double *d) { char *e; *d=strtod(s,&e); return e!=s && !*e; }
static void out(double d) { if (isfinite(d)) printf("%.17g",d); else printf("null"); }
int main(void)
{
    char line[8192];
    const deco_tissues_constants *c=deco_tissues_get_constants();
    printf("{\"policy_id\":\"%s\",\"coefficient_id\":\"%s\",\"coefficient_sha256\":\"%s\"}\n",c->policy_id,c->coefficient_id,c->coefficient_sha256);
    while (fgets(line,sizeof(line),stdin)) {
        char *t[24],*s,*e; size_t n=0,i; uint64_t allowance;
        double surface,start,dest,anchor=0,before_anchor;
        deco_tissues_state state,before;
        deco_transit_policy policy={DECO_TRANSIT_FIXED_GF,0,NULL};
        deco_transit_workspace workspace;
        deco_transit_result r;
        if (!strchr(line,'\n') && !feof(stdin)) return 2;
        for(s=strtok(line," \t\r\n");s;s=strtok(NULL," \t\r\n")) { if(n==24)return 2;t[n++]=s; }
        if(n!=23 || !num(t[1],&surface) || !num(t[2],&start) || !num(t[3],&dest))return 2;
        if(!strcmp(t[0],"fixed")) { if(!num(t[4],&policy.fixed_gf))return 2; }
        else if(!strcmp(t[0],"line")) {
            policy.mode=DECO_TRANSIT_ANCHOR_LINE;
            if(strcmp(t[4],"none")) { if(!num(t[4],&anchor))return 2;policy.retained_anchor_bar_abs=&anchor; }
        } else return 2;
        for(s=t[5];*s;s++)if(*s<'0'||*s>'9')return 2;
        errno=0;allowance=strtoull(t[5],&e,10);if(errno||e==t[5]||*e)return 2;
        if(strlen(t[6])!=1 || t[6][0]<'0' || t[6][0]>'2')return 2;
        if(deco_tissues_init_fresh(&state,1)!=DECO_TISSUES_OK)return 2;
        if(t[6][0]=='1')state.policy_id[0]^=1;
        if(t[6][0]=='2')state.coefficient_sha256[0]^=1;
        for(i=0;i<16;i++)if(!num(t[7+i],&state.nitrogen_bar[i]))return 2;
        memcpy(&before,&state,sizeof(state));memcpy(&before_anchor,&anchor,sizeof(anchor));
        r=deco_transit_evaluate(&state,start,dest,surface,&policy,allowance,&workspace);
        printf("{\"status\":%d,\"inspected_points\":%" PRIu64 ",\"arrival_seconds\":",r.status,r.inspected_points);out(r.arrival_seconds);
        printf(",\"input_unchanged\":%s,\"anchor_unchanged\":%s,\"diagnostic_valid\":%s,\"destination_valid\":%s,\"failure_seconds\":",memcmp(&before,&state,sizeof(state))?"false":"true",memcmp(&before_anchor,&anchor,sizeof(anchor))?"false":"true",r.diagnostic_valid?"true":"false",r.destination_valid?"true":"false");out(r.failure_seconds);
        printf(",\"failure_pressure_bar_abs\":");out(r.failure_pressure_bar_abs);
        printf(",\"failure_gf\":");out(r.failure_gf);
        printf(",\"failure_ceiling_bar_abs\":");out(r.failure_ceiling_bar_abs);
        printf(",\"failure_margin_bar\":");out(r.failure_margin_bar);
        printf(",\"destination_nitrogen_bar\":");
        if(!r.destination_valid)printf("null");else {printf("[");for(i=0;i<16;i++){if(i)printf(",");out(r.destination.nitrogen_bar[i]);}printf("]");}
        puts("}");
    }
    return ferror(stdin)?2:0;
}
