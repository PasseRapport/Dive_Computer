/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1);} }while(0)
static ostc4_observation matched_control(bool intervene)
{
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    CHECK(pc_bench_set_gf_pair(40,85)==0);
    if(intervene) {
        CHECK(pc_bench_gf_edit_begin()==0);
        CHECK(pc_bench_gf_edit_adjust(false,1)==0);
        CHECK(pc_bench_gf_edit_adjust(true,-1)==0);
        pc_bench_gf_edit_cancel();
        CHECK(pc_bench_set_gf_pair(86,85)==-1);
    }
    CHECK(pc_bench_accept_sample(&(ostc4_sample){120,4.01325f})==0);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){2220,4.01325f})==0);
    if(intervene) {
        unsigned char before[OSTC4_CHECKPOINT_BYTES],after[OSTC4_CHECKPOINT_BYTES];
        CHECK(ostc4_exposure_checkpoint(before,sizeof(before))==OSTC4_OK);
        ostc4_observation prediction;
        CHECK(ostc4_exposure_predict(&(ostc4_sample){2400,1.61325f},&prediction)==OSTC4_OK);
        CHECK(ostc4_exposure_predict(&(ostc4_sample){2220,1.01325f},&prediction)==OSTC4_BAD_TIME);
        CHECK(ostc4_exposure_set_gf_pair(20,60)==OSTC4_BAD_INPUT);
        CHECK(ostc4_exposure_checkpoint(after,sizeof(after))==OSTC4_OK);
        CHECK(!memcmp(before,after,sizeof(before)));
    }
    CHECK(pc_bench_accept_sample(&(ostc4_sample){2380,1.61325f})==0);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){2680,1.61325f})==0);
    return pc_bench_read()->result;
}
int main(void)
{
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    const struct bench_view *v=pc_bench_read();
    CHECK(v->preferred_gf.low==30 && v->preferred_gf.high==70);
    ostc4_observation before=v->result;
    CHECK(pc_bench_set_gf_pair(70,70)==0);
    CHECK(v->preferred_gf.low==70 && v->preferred_gf.high==70);
    CHECK(v->result.gf_pair.low==70 && v->result.gf_pair.high==70);
    CHECK(v->result.timestamp_seconds==before.timestamp_seconds);
    CHECK(!memcmp(v->result.nitrogen_bar,before.nitrogen_bar,sizeof(before.nitrogen_bar)));
    before=v->result;
    const int invalid[][2]={{0,70},{30,101},{71,70},{-1,70},{256,256}};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        CHECK(pc_bench_set_gf_pair(invalid[i][0],invalid[i][1])==-1);
        CHECK(!memcmp(&before,&v->result,sizeof(before)));
        CHECK(v->preferred_gf.low==70 && v->preferred_gf.high==70);
    }
    CHECK(pc_bench_gf_edit_begin()==0);
    CHECK(pc_bench_gf_edit_adjust(false,-10)==0);
    CHECK(v->draft_gf.low==60 && v->preferred_gf.low==70);
    pc_bench_gf_edit_cancel();
    CHECK(!v->gf_editing && !memcmp(&before,&v->result,sizeof(before)));
    CHECK(pc_bench_set_gf_pair(30,70)==0);
    CHECK(pc_bench_gf_edit_begin()==0);
    CHECK(pc_bench_gf_edit_adjust(true,10)==0);
    CHECK(v->draft_gf.high==80 && v->preferred_gf.high==70);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0);
    CHECK(v->recording.active && v->gf_locked && v->result.gf_locked);
    CHECK(!v->gf_editing && v->active_gf.low==30 && v->active_gf.high==70);
    before=v->result;
    CHECK(pc_bench_set_gf_pair(20,60)==-1);
    CHECK(ostc4_exposure_set_gf_pair(20,60)==OSTC4_BAD_INPUT);
    CHECK(pc_bench_gf_edit_begin()==-1);
    CHECK(!memcmp(&before,&v->result,sizeof(before)));
    CHECK(pc_bench_accept_sample(&(ostc4_sample){90,1.01325f})==0);
    CHECK(v->gf_locked); /* Brief surfacing never unlocks. */
    CHECK(pc_bench_set_gf_pair(20,60)==-1);
    CHECK(pc_bench_recording_finish()==0);
    CHECK(!v->gf_locked && !v->result.gf_locked);
    before=v->result;
    size_t history_count=v->history_count;
    CHECK(pc_bench_set_gf_pair(20,60)==0);
    CHECK(v->history_count==history_count && v->seconds==90);
    CHECK(!memcmp(before.nitrogen_bar,v->result.nitrogen_bar,sizeof(before.nitrogen_bar)));
    pc_bench_recording_arm();
    CHECK(pc_bench_accept_sample(&(ostc4_sample){120,1.31325f})==0);
    CHECK(v->active_gf.low==20 && v->active_gf.high==60 && v->gf_locked);
    CHECK(v->recording.dives_started==2);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){150,1.01325f})==0);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){450,1.01325f})==0);
    CHECK(!v->recording.active && !v->gf_locked && !v->result.gf_locked);
    CHECK(pc_bench_gf_edit_begin()==0);
    CHECK(pc_bench_gf_edit_adjust(false,1000)==0);
    CHECK(v->draft_gf.low==100 && v->draft_gf.high==60);
    before=v->result;
    CHECK(pc_bench_gf_edit_commit()==-1);
    CHECK(v->gf_editing && !memcmp(&before,&v->result,sizeof(before)));
    pc_bench_gf_edit_cancel();
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    CHECK(pc_bench_set_gf_pair(40,85)==0);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0);
    CHECK(pc_bench_checkpoint()==0);
    before=v->result;
    CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
    CHECK(!memcmp(&before,&v->result,sizeof(before)));
    CHECK(v->recording.active && v->gf_locked && v->active_gf.low==40);
    CHECK(ostc4_exposure_set_gf_pair(30,70)==OSTC4_BAD_INPUT);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){90,1.01325f})==0);
    CHECK(pc_bench_accept_sample(&(ostc4_sample){390,1.01325f})==0);
    CHECK(!v->recording.active && !v->gf_locked);
    CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTORED);
    CHECK(!v->recording.active && !v->gf_locked && v->result.gf_pair.low==40);
    CHECK(pc_bench_set_gf_pair(20,60)==0);
    CHECK(strstr(v->recovery,"configuration")!=NULL);
    CHECK(pc_bench_restart(PC_RESTART_KNOWN)==OSTC4_RESTART_CONFIGURATION_UNKNOWN);
    CHECK(v->result.tissues==OSTC4_INVALID);
    ostc4_observation control=matched_control(false),intervened=matched_control(true);
    CHECK(!memcmp(&control,&intervened,sizeof(control)));
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    CHECK(pc_bench_set_gf_pair(1,1)==0);
    CHECK(pc_bench_gf_edit_begin()==0);
    CHECK(pc_bench_gf_edit_adjust(true,1)==0);
    CHECK(v->draft_gf.high==2 && v->preferred_gf.high==1);
    CHECK(pc_bench_gf_edit_commit()==0);
    CHECK(!v->gf_editing && v->preferred_gf.low==1 && v->preferred_gf.high==2);
    CHECK(pc_bench_set_gf_pair(1,100)==0);
    CHECK(pc_bench_set_gf_pair(100,100)==0);
    puts("atomic GF editing, recording lock, checkpoint, matched-control/prediction isolation pass");
}
