/* SURFACE CURRENT DURATION_MS ANCHOR|none DWELL_MS CANDIDATES POINTS
 * IDENTITY P0 ... P15. Identity: 0 compatible, 1 bad policy, 2 bad hash.
 * Identity header then JSON result per line; malformed input exits 2. */
#include "deco_anchor.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int number(const char *s, double *d) { char *e; *d=strtod(s,&e); return e!=s && !*e; }
static int integer(const char *s, uint64_t *v) {
    const char *p; char *e;
    if(!*s)return 0;
    for(p=s;*p;p++)if(*p<'0'||*p>'9')return 0;
    errno=0;*v=strtoull(s,&e,10);return !errno && e!=s && !*e;
}
static void out(double d) { if(isfinite(d))printf("%.17g",d);else printf("null"); }
int main(void)
{
    char line[8192];
    const deco_tissues_constants *c=deco_tissues_get_constants();
    printf("{\"policy_id\":\"%s\",\"coefficient_id\":\"%s\",\"coefficient_sha256\":\"%s\"}\n",c->policy_id,c->coefficient_id,c->coefficient_sha256);
    while(fgets(line,sizeof(line),stdin)) {
        char *tokens[24],*p; size_t n=0,i;
        double surface,current; uint64_t duration;
        deco_tissues_state tissues,before;
        deco_anchor_state prior={0},saved;
        deco_anchor_allowance budget,saved_budget;
        deco_anchor_workspace workspace;
        deco_anchor_result result;
        if(!strchr(line,'\n')&&!feof(stdin))return 2;
        for(p=strtok(line," \t\r\n");p;p=strtok(NULL," \t\r\n")) {
            if(n==24)return 2;
            tokens[n++]=p;
        }
        if(n!=24 || !number(tokens[0],&surface) || !number(tokens[1],&current) ||
           !integer(tokens[2],&duration) || !integer(tokens[4],&prior.surface_dwell_ms) ||
           !integer(tokens[5],&budget.candidates) || !integer(tokens[6],&budget.points))return 2;
        if(strcmp(tokens[3],"none")) {
            prior.present=true;if(!number(tokens[3],&prior.pressure_bar_abs))return 2;
        }
        if(strlen(tokens[7])!=1||tokens[7][0]<'0'||tokens[7][0]>'2')return 2;
        if(deco_tissues_init_fresh(&tissues,1)!=DECO_TISSUES_OK)return 2;
        if(tokens[7][0]=='1')tissues.policy_id[0]^=1;
        if(tokens[7][0]=='2')tissues.coefficient_sha256[0]^=1;
        for(i=0;i<16;i++)if(!number(tokens[8+i],&tissues.nitrogen_bar[i]))return 2;
        memcpy(&before,&tissues,sizeof(tissues));memcpy(&saved,&prior,sizeof(prior));
        memcpy(&saved_budget,&budget,sizeof(budget));
        result=deco_anchor_transition(&tissues,current,surface,duration,&prior,&budget,&workspace);
        printf("{\"status\":%d,\"change\":%d,\"candidate_valid\":%s,\"ceiling_valid\":%s,\"violated\":%s,",result.status,result.change,result.candidate_valid?"true":"false",result.ceiling_valid?"true":"false",result.violated?"true":"false");
        printf("\"attempted_candidates\":%" PRIu64 ",\"inspected_points\":%" PRIu64 ",",result.attempted_candidates,result.inspected_points);
        printf("\"input_unchanged\":%s,\"anchor_unchanged\":%s,\"allowance_unchanged\":%s,",memcmp(&before,&tissues,sizeof(tissues))?"false":"true",memcmp(&saved,&prior,sizeof(prior))?"false":"true",memcmp(&saved_budget,&budget,sizeof(budget))?"false":"true");
        printf("\"candidate\":");
        if(!result.candidate_valid)printf("null");else {
            printf("{\"present\":%s,\"pressure_bar_abs\":",result.candidate.present?"true":"false");out(result.candidate.pressure_bar_abs);
            printf(",\"surface_dwell_ms\":%" PRIu64 "}",result.candidate.surface_dwell_ms);
        }
        printf(",\"gf\":");out(result.gf);
        printf(",\"local_ceiling_bar_abs\":");out(result.local_ceiling_bar_abs);puts("}");
    }
    return ferror(stdin)?2:0;
}
