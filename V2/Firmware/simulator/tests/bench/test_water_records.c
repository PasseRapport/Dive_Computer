/* Public PC configuration, exposure and record observation; no private state. */
#define _POSIX_C_SOURCE 200809L
#include "pc_bench.h"
#include "pc_bench_checkpoint_file.h"
#include "sim_host.h"
#include "water_settings.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void sample(uint32_t t,float p){CHECK(pc_bench_accept_sample(&(ostc4_sample){t,p})==0);}
int main(int argc,char **argv)
{
    CHECK(argc==3);CHECK(!setenv("DIVE_WATCH_STATE_DIR",argv[1],1));
    CHECK(pc_bench_storage_configure(argv[1])==0);
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
    if(!strcmp(argv[2],"save")) {
        CHECK(pc_bench_set_gf_pair(40,85)==0);
        sample(60,2.96133f);
        CHECK(pc_bench_read()->recording.active);
        CHECK(pc_bench_checkpoint_file_save(pc_bench_checkpoint_file_default_path())==0);
        CHECK(sim_host_water_preferences_save(1)==0);
        return 0;
    }
    if(!strcmp(argv[2],"resume")) {
        int salt=0;CHECK(sim_host_water_preferences_load(&salt)==0 && salt==1);
        CHECK(pc_bench_set_water(true)==0);
        CHECK(pc_bench_checkpoint_file_load_saved_instant(pc_bench_checkpoint_file_default_path())==OSTC4_RESTORED);
        const struct bench_view *v=pc_bench_read();
        CHECK(v->recording.active && water_settings_locked() && !water_settings_salt());
        CHECK(v->density_kg_m3==1000 && v->recording.density_kg_m3==1000);
        CHECK(pc_bench_set_water(true)==-1);
        CHECK(pc_bench_checkpoint()==0);
        CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
        CHECK(v->density_kg_m3==1000 && !water_settings_salt());
        /* Fresh0.499m is Marine0.486829268m; binary32 depth tolerance1e-5m. */
        sample(120,1.0489351835f);CHECK(v->recording.active && !water_settings_salt());
        CHECK(pc_bench_recording_finish()==0);
        CHECK(!water_settings_locked() && water_settings_salt() && v->density_kg_m3==1025);
        CHECK(fabsf(v->depth_m-.486829268f)<1e-5f);
        struct pc_bench_log log;CHECK(pc_bench_log_read(0,&log)==0 && log.density_kg_m3==1000);
        CHECK(sim_host_water_preferences_load(&salt)==0 && salt==1);
        pc_bench_recording_arm();pc_bench_manual(20,0);pc_bench_pause(false);pc_bench_poll(1000);
        /* Independent Marine20m input, absolute pressure binary32 tolerance1e-6bar. */
        CHECK(fabsf(v->result.absolute_pressure_bar-3.01036325f)<1e-6f);
        CHECK(v->recording.active && v->recording.density_kg_m3==1025);
        sample(240,1);CHECK(pc_bench_recording_finish()==0);
        CHECK(pc_bench_log_read(1,&log)==0 && log.density_kg_m3==1025);
        return 0;
    }
    if(!strcmp(argv[2],"cross-records")) {
        CHECK(pc_bench_set_water(true)==0);CHECK(pc_bench_set_gf_pair(20,90)==0);
        CHECK(pc_bench_checkpoint_file_load_saved_instant(pc_bench_checkpoint_file_default_path())==OSTC4_RESTORED);
        sample(61,1);sample(359,1);
        const struct bench_view *v=pc_bench_read();CHECK(v->recording.surface_seconds==299);
        sample(419,1.5f); /* completes old dive then starts next within one accepted segment */
        struct pc_bench_log log;CHECK(pc_bench_log_read(0,&log)==0);
        CHECK(log.completed_seconds==360 && log.density_kg_m3==1000 && log.max_depth_dm==200);
        CHECK(log.active_gf.low==40 && log.active_gf.high==85);
        CHECK(v->recording.active && v->recording.started_seconds==384 && v->recording.density_kg_m3==1025);
        CHECK(v->active_gf.low==20 && v->result.gf_pair.low==20 && v->gf_locked);
        return 0;
    }
    if(!strcmp(argv[2],"memory")) {
        CHECK(pc_bench_set_water(true)==0);
        CHECK(pc_bench_checkpoint_file_load_saved_instant(pc_bench_checkpoint_file_default_path())==OSTC4_RESTORED);
        CHECK(pc_bench_checkpoint()==0);
        /* Fresh0.503m stays underwater, Marine0.49073m would count surface time. */
        sample(61,1.0493264495f);sample(361,1.0493264495f);
        sample(421,1);sample(721,1);
        const struct bench_view *v=pc_bench_read();
        CHECK(!v->recording.active && v->density_kg_m3==1025);
        ostc4_observation result=v->result;struct recorded_dive recording=v->recording;
        CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
        CHECK(!memcmp(&result,&v->result,sizeof(result)));
        CHECK(!memcmp(&recording,&v->recording,sizeof(recording)));
        CHECK(v->storage->log_count==1);
        struct pc_bench_log log;CHECK(pc_bench_log_read(0,&log)==0 && log.density_kg_m3==1000);
        return 0;
    }
    if(!strcmp(argv[2],"gap") || !strcmp(argv[2],"clock") || !strcmp(argv[2],"incompatible")) {
        CHECK(pc_bench_set_water(true)==0);
        ostc4_recovery evidence={true,120,86400,NULL,0};
        bool clock=!strcmp(argv[2],"clock"),incompatible=!strcmp(argv[2],"incompatible");
        if(clock)evidence.time_known=false;
        ostc4_restart_status expected=incompatible?OSTC4_CHECKPOINT_INCOMPATIBLE:
            clock?OSTC4_RESTART_TIME_UNKNOWN:OSTC4_RESTART_EXPOSURE_UNKNOWN;
        CHECK(pc_bench_checkpoint_file_load(pc_bench_checkpoint_file_default_path(),&evidence,true)==expected);
        CHECK(pc_bench_read()->gap && pc_bench_read()->result.tissues==OSTC4_INVALID);
        int salt=0;CHECK(sim_host_water_preferences_load(&salt)==0 && salt==1);
        CHECK(pc_bench_read()->result.tissues==OSTC4_INVALID);
        CHECK(pc_bench_read()->storage->log_count==0);
        return 0;
    }
    if(!strcmp(argv[2],"legacy")) {
        struct pc_bench_log old;
        CHECK(pc_bench_read()->storage->log_count==4);
        for(size_t i=2;i<4;++i) {
            CHECK(pc_bench_log_read(i,&old)==0);
            CHECK(!old.density_kg_m3 && !old.max_depth_dm);
        }
        CHECK(pc_bench_set_water(true)==0);
        return 0;
    }
    if(!strcmp(argv[2],"record")) {
        sample(60,2.96133f);sample(120,1);
        CHECK(pc_bench_recording_finish()==0);
        CHECK(pc_bench_set_water(true)==0);
        pc_bench_recording_arm();sample(180,3.01036325f);sample(240,1);
        CHECK(pc_bench_recording_finish()==0);
        CHECK(sim_host_water_preferences_save(1)==0);
    }
    struct pc_bench_log record;
    CHECK(pc_bench_read()->storage->log_count==2);
    CHECK(pc_bench_log_read(0,&record)==0 && record.density_kg_m3==1000 && record.max_depth_dm==200);
    CHECK(pc_bench_log_read(1,&record)==0 && record.density_kg_m3==1025 && record.max_depth_dm==200);
    return 0;
}
