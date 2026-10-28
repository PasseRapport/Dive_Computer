/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_checkpoint_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void sample(uint32_t t,float p){CHECK(pc_bench_accept_sample(&(ostc4_sample){t,p})==0);}
static void fixture(void)
{CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(40,85)==0);sample(60,4);sample(1260,4);}
static void snapshot(void)
{
    const struct bench_view *v=pc_bench_read();const ostc4_observation *r=&v->result;
    printf("%u %u %a %a %a %d %d %a %u %u %u %u %u %u %u %u %u %u %d %d %d %d %d",v->seconds,r->dive_seconds,
        (double)r->absolute_pressure_bar,(double)r->gf_anchor_ambient_bar,(double)r->gf_low_depth_bar,
        r->ndl_seconds,r->tts_seconds,(double)r->ceiling_meters,r->gf_pair.low,r->gf_pair.high,
        (unsigned)v->gf_locked,(unsigned)v->recording.active,v->recording.started_seconds,v->recording.completed_seconds,
        v->recording.dive_seconds,v->recording.surface_seconds,v->recording.dives_started,v->recording.dives_completed,
        r->tissues,r->ndl,r->tts,r->ceiling,r->stops);
    for(int i=0;i<16;++i)printf(" %a %a",(double)r->nitrogen_bar[i],(double)r->helium_bar[i]);
    for(int i=0;i<OSTC4_STOPS;++i)printf(" %u",r->stop_seconds[i]);
    puts("");
}
static void continuation(void)
{snapshot();sample(1500,1);snapshot();sample(1796,1);snapshot();sample(1797,1);snapshot();
 CHECK(pc_bench_set_gf_pair(20,90)==0);sample(1857,3);snapshot();sample(2457,3);snapshot();}
int main(int argc,char **argv)
{
    CHECK(argc>=3);
    if(!strcmp(argv[1],"control")){fixture();sample(1320,4);continuation();return 0;}
    if(!strcmp(argv[1],"resume")) {
        ostc4_sample later={1320,4};ostc4_recovery evidence={true,1320,86400,&later,1};
        CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTORED);
        continuation();return 0;
    }
    if(!strcmp(argv[1],"fault")) {
        CHECK(argc==5);fixture();ostc4_observation before=pc_bench_read()->result;
        ostc4_recovery evidence={true,1260,86400,NULL,0};
        if(!strcmp(argv[4],"clock"))evidence.time_known=false;
        if(!strcmp(argv[4],"gap"))evidence.restart_seconds=1320;
        if(!strcmp(argv[4],"stale")){evidence.restart_seconds=1320;evidence.max_age_seconds=0;}
        ostc4_restart_status expected=(ostc4_restart_status)strtoul(argv[3],NULL,10);
        CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==expected);
        const struct bench_view *v=pc_bench_read();CHECK(v->result.tissues==OSTC4_INVALID && v->result.ndl==OSTC4_INVALID);
        CHECK(v->result.tts==OSTC4_INVALID && v->result.ceiling==OSTC4_INVALID && v->result.stops==OSTC4_INVALID);
        CHECK(v->gf_locked && v->active_gf.low==40 && v->seconds==1260);
        CHECK(!memcmp(before.nitrogen_bar,v->result.nitrogen_bar,sizeof(before.nitrogen_bar)));
        CHECK(pc_bench_set_gf_pair(20,90)==-1);return 0;
    }
    if(!strcmp(argv[1],"legacy")) {
        ostc4_recovery evidence={true,1260,86400,NULL,0};
        CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTART_CONFIGURATION_UNKNOWN);
        const struct bench_view *v=pc_bench_read();CHECK(v->gf_locked && v->active_gf.low==30 && v->active_gf.high==70);
        CHECK(v->result.tissues==OSTC4_INVALID && v->seconds==1260 && v->result.nitrogen_bar[0]>2);
        CHECK(strstr(v->recovery,"Legacy") && strstr(v->recovery,"UNKNOWN"));
        CHECK(pc_bench_set_gf_pair(40,85)==-1);CHECK(pc_bench_recording_finish()==-1);
        CHECK(pc_bench_checkpoint_file_save(argv[2])==-1);return 0;
    }
    if(!strcmp(argv[1],"write-failure")) {
        fixture();ostc4_observation before=pc_bench_read()->result;
        CHECK(pc_bench_checkpoint_file_save(argv[2])==-1);
        CHECK(!memcmp(&before,&pc_bench_read()->result,sizeof(before)));
        CHECK(pc_bench_read()->gf_locked && strstr(pc_bench_read()->recovery,"failed"));return 0;
    }
    if(!strcmp(argv[1],"preferred")) {
        CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(20,90)==0);
        CHECK(pc_bench_checkpoint_file_load_saved_instant(argv[2])==OSTC4_RESTORED);
        CHECK(pc_bench_read()->preferred_gf.low==20 && pc_bench_read()->active_gf.low==40);
        CHECK(pc_bench_set_gf_pair(20,90)==-1);sample(1440,1);
        CHECK(pc_bench_recording_finish()==0);CHECK(!pc_bench_read()->gf_locked);
        CHECK(pc_bench_set_gf_pair(20,90)==0);return 0;
    }
    if(!strcmp(argv[1],"save")) {
        CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(40,85)==0);
        sample(60,4);sample(1260,4);CHECK(pc_bench_read()->gf_locked);
        CHECK(pc_bench_checkpoint_file_save(argv[2])==0);return 0;
    }
    if(!strcmp(argv[1],"continued")) {
        ostc4_sample later[]={{1440,1},{1736,1},{1737,1}};
        ostc4_recovery evidence={true,1737,86400,later,3};
        CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTORED);
        CHECK(!pc_bench_read()->gf_locked && pc_bench_read()->recording.dives_completed==1);
        return 0;
    }
    if(!strcmp(argv[1],"configuration-unknown")) {
        ostc4_sample later={1440,1};ostc4_recovery evidence={true,1440,86400,&later,1};
        CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,false)==OSTC4_RESTART_CONFIGURATION_UNKNOWN);
        CHECK(pc_bench_read()->result.tissues==OSTC4_INVALID);return 0;
    }
    CHECK(!strcmp(argv[1],"load"));
    ostc4_recovery evidence={true,1260,86400,NULL,0};
    CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTORED);
    CHECK(pc_bench_read()->active_gf.low==40 && pc_bench_read()->active_gf.high==85);
    CHECK(pc_bench_read()->gf_locked && pc_bench_read()->recording.active);
    sample(1440,1);CHECK(pc_bench_read()->gf_locked);
    sample(1736,1);CHECK(pc_bench_read()->gf_locked);
    sample(1737,1);CHECK(!pc_bench_read()->gf_locked);
    CHECK(pc_bench_set_gf_pair(20,90)==0);
    puts("fresh-process checkpoint pair, lifecycle and unlock PASS");return 0;
}
