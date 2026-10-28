/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_checkpoint_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void sample(uint32_t t,float p){CHECK(pc_bench_accept_sample(&(ostc4_sample){t,p})==0);}
static void later_exposure(void)
{sample(1440,1);sample(1736,1);sample(1737,1);sample(1797,3);sample(2457,3);sample(2637,1);sample(2937,1);}
static void replay_checks(const char *state,const char *checkpoint,const char *mode)
{
    CHECK(pc_bench_storage_configure(state)==0);
    if(!strcmp(mode,"legacy-read")) {
        struct pc_bench_log old;
        CHECK(pc_bench_read()->storage->log_count==1 && pc_bench_log_read(0,&old)==0);
        CHECK(!old.recording_id && old.active_gf.low==30 && old.active_gf.high==70);
        CHECK(old.started_seconds==40 && old.completed_seconds==90);return;
    }
    if(!strcmp(mode,"reopen")) {
        ostc4_sample later[]={{1440,1},{1736,1},{1737,1},{1797,3},{2457,3},{2637,1},{2937,1}};
        ostc4_recovery evidence={true,2937,86400,later,7};
        CHECK(pc_bench_checkpoint_file_load(checkpoint,&evidence,true)==OSTC4_RESTORED);
        CHECK(pc_bench_read()->storage->log_count==2 && pc_bench_read()->recording.dives_completed==2);
        CHECK(pc_bench_read()->active_gf.low==20 && !pc_bench_read()->gf_locked);return;
    }
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(40,85)==0);
    sample(60,4);sample(1260,4);CHECK(pc_bench_checkpoint_file_save(checkpoint)==0);
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(20,90)==0);
    CHECK(pc_bench_checkpoint_file_load_saved_instant(checkpoint)==OSTC4_RESTORED);
    CHECK(pc_bench_checkpoint_file_save(checkpoint)==0);CHECK(pc_bench_checkpoint()==0);
    later_exposure();
    ostc4_observation uninterrupted=pc_bench_read()->result;
    struct recorded_dive recording=pc_bench_read()->recording;
    CHECK(recording.dives_completed==2 && !pc_bench_read()->gf_locked);
    CHECK(pc_bench_read()->storage->log_count==2);
    struct pc_bench_log first,second;CHECK(pc_bench_log_read(0,&first)==0 && pc_bench_log_read(1,&second)==0);
    CHECK(first.active_gf.low==40 && second.active_gf.low==20 && first.recording_id!=second.recording_id);
    if(!strcmp(mode,"memory")) {
        CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
        CHECK(!memcmp(&uninterrupted,&pc_bench_read()->result,sizeof(uninterrupted)));
        CHECK(!memcmp(&recording,&pc_bench_read()->recording,sizeof(recording)));
        CHECK(pc_bench_read()->storage->log_count==2);return;
    }
    ostc4_sample later[]={{1440,1},{1736,1},{1737,1},{1797,3},{2457,3},{2637,1},{2937,1}};
    ostc4_recovery evidence={true,2937,86400,later,7};
    for(unsigned repeat=0;repeat<2;++repeat) {
        CHECK(pc_bench_checkpoint_file_load(checkpoint,&evidence,true)==OSTC4_RESTORED);
        CHECK(!memcmp(&uninterrupted,&pc_bench_read()->result,sizeof(uninterrupted)));
        CHECK(!memcmp(&recording,&pc_bench_read()->recording,sizeof(recording)));
        CHECK(pc_bench_read()->storage->log_count==2);
    }
    struct pc_bench_log preserved;CHECK(pc_bench_log_read(0,&preserved)==0);
    CHECK(!memcmp(&first,&preserved,sizeof(first)));
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);
    if(argc==4){replay_checks(argv[1],argv[2],argv[3]);return 0;}
    CHECK(pc_bench_storage_configure(argv[1])==0);
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(40,85)==0);
    sample(60,4);sample(1260,4);CHECK(pc_bench_checkpoint_file_save(argv[2])==0);
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(20,90)==0);
    CHECK(pc_bench_checkpoint_file_load_saved_instant(argv[2])==OSTC4_RESTORED);
    CHECK(pc_bench_read()->preferred_gf.low==20 && pc_bench_read()->active_gf.low==40);
    CHECK(pc_bench_checkpoint()==0);CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
    CHECK(pc_bench_read()->active_gf.low==40 && pc_bench_read()->active_gf.high==85);
    CHECK(pc_bench_read()->result.gf_pair.low==40 && pc_bench_read()->preferred_gf.low==20);
    sample(1440,1);CHECK(pc_bench_recording_finish()==0);
    struct pc_bench_log log;CHECK(pc_bench_log_read(0,&log)==0);
    CHECK(log.active_gf.low==40 && log.active_gf.high==85);
    puts("RECOVERY GF/LOG PASS: active pair survives memory recovery independently of preferred pair");
    ostc4_sample completion[]={{1440,1},{1736,1},{1737,1}};
    ostc4_recovery evidence={true,1737,86400,completion,3};
    CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTORED);
    CHECK(pc_bench_read()->recording.dives_completed==1);
    CHECK(pc_bench_read()->storage->log_count==2);
    CHECK(pc_bench_log_read(1,&log)==0 && log.automatic_completion);
    CHECK(log.active_gf.low==40 && log.active_gf.high==85 && log.completed_seconds==1737);
    CHECK(pc_bench_checkpoint_file_load(argv[2],&evidence,true)==OSTC4_RESTORED);
    CHECK(pc_bench_read()->storage->log_count==2);
    for(unsigned repeat=0;repeat<2;++repeat) {
        CHECK(pc_bench_fresh(1,1000,NULL,0)==0);CHECK(pc_bench_set_gf_pair(40,85)==0);
        sample(60,4);sample(1260,4);sample(1440,1);sample(1736,1);sample(1737,1);
    }
    CHECK(pc_bench_read()->storage->log_count==4);
    CHECK(pc_bench_checkpoint_file_load_saved_instant(argv[2])==OSTC4_RESTORED);
    sample(1440,1);CHECK(pc_bench_recording_finish()==0);
    CHECK(pc_bench_read()->storage->log_count==4); /* same recovered manual completion, same immutable record */
    struct pc_bench_log a,b;CHECK(pc_bench_log_read(2,&a)==0 && pc_bench_log_read(3,&b)==0);
    CHECK(a.recording_id && b.recording_id && a.recording_id!=b.recording_id);
    return 0;
}
