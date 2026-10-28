/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "scuba_calculated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1);} }while(0)
int main(void)
{
    CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
    pc_bench_poll(1920000);
    CHECK(pc_bench_checkpoint()==0);
    pc_bench_poll(1680000);
    ostc4_observation before=pc_bench_read()->result;
    CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
    CHECK(memcmp(&before,&pc_bench_read()->result,sizeof(before))==0);
    CHECK(pc_bench_read()->paused && !pc_bench_read()->automatic && !pc_bench_read()->replay);
    CHECK(pc_bench_read()->history_count==3601);
    const enum pc_restart_mode modes[]={PC_RESTART_GAP,PC_RESTART_CLOCK,PC_RESTART_CORRUPT,
        PC_RESTART_INCOMPATIBLE,PC_RESTART_STALE};
    const ostc4_restart_status expected[]={OSTC4_RESTART_EXPOSURE_UNKNOWN,OSTC4_RESTART_TIME_UNKNOWN,
        OSTC4_CHECKPOINT_CORRUPT,OSTC4_CHECKPOINT_INCOMPATIBLE,OSTC4_CHECKPOINT_STALE};
    for(size_t i=0;i<sizeof(modes)/sizeof(*modes);++i) {
        CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
        pc_bench_poll(1920000);CHECK(pc_bench_checkpoint()==0);pc_bench_poll(180000);
        CHECK(pc_bench_restart(modes[i])==expected[i]);
        const struct bench_view *v=pc_bench_read();
        CHECK(v->gap && v->error && v->result.tissues==OSTC4_INVALID);
        CHECK(v->data_gap.present==(i<2));
        if(i==0)CHECK(v->data_gap.time_known && !v->data_gap.ongoing && v->data_gap.duration_seconds==180);
        if(i==1)CHECK(!v->data_gap.time_known);
        struct scuba_model model;scuba_model_init(&model);
        scuba_calculated_apply(&model,&v->result,v->seconds,v->depth_m,18,87);
        CHECK(model.calculated_state==SCUBA_CALCULATED_INVALID);
        pc_bench_pause(false);pc_bench_poll(1000);
        CHECK(v->seconds==2100 && v->result.tissues==OSTC4_INVALID);
        CHECK(pc_bench_auto(true)==-1 && pc_bench_checkpoint()==-1);
    }
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    CHECK(pc_bench_read()->result.tissues==OSTC4_AVAILABLE);
    CHECK(!pc_bench_read()->error && !pc_bench_read()->gap);
    CHECK(pc_bench_checkpoint()==0);
    pc_bench_mark_missing(); /* Unknown duration, even though simulation clock did not advance. */
    CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTART_EXPOSURE_UNKNOWN);
    CHECK(pc_bench_read()->result.tissues==OSTC4_INVALID);
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    pc_bench_manual(10,0);pc_bench_poll(60000);
    ostc4_observation trusted=pc_bench_read()->result;
    CHECK(pc_bench_checkpoint()==0);
    pc_bench_sensor_loss();
    const struct bench_view *v=pc_bench_read();
    CHECK(v->data_gap.present && v->data_gap.time_known && v->data_gap.ongoing);
    CHECK(v->data_gap.start_seconds==60 && v->data_gap.duration_seconds==0);
    pc_bench_poll(125000);
    CHECK(v->elapsed_seconds==185 && v->seconds==60 && v->data_gap.duration_seconds==125);
    CHECK(v->result.timestamp_seconds==trusted.timestamp_seconds);
    CHECK(memcmp(v->result.nitrogen_bar,trusted.nitrogen_bar,sizeof(trusted.nitrogen_bar))==0);
    CHECK(v->history_count==61 && v->result.tissues==OSTC4_INVALID);
    pc_bench_sensor_loss(); /* Repeated clicks do not erase outage duration. */
    CHECK(v->data_gap.duration_seconds==125);
    pc_bench_pause(true);pc_bench_poll(100000);CHECK(v->elapsed_seconds==185);
    pc_bench_pause(false);pc_bench_speed(10);pc_bench_poll(1000);
    CHECK(v->elapsed_seconds==195 && v->data_gap.duration_seconds==135);
    CHECK(pc_bench_restart(PC_RESTART_CLOCK)==OSTC4_RESTART_TIME_UNKNOWN);
    CHECK(!v->data_gap.time_known);
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    CHECK(!v->data_gap.present && v->elapsed_seconds==0);
    puts("restart diagnostics and calculated watch validity pass");
    return 0;
}
