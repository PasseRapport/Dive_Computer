/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1);} }while(0)
int main(int argc,char **argv)
{
    CHECK(argc>=3);
    CHECK(pc_bench_storage_configure(argv[1])==0);
    CHECK(pc_bench_fresh(1.01325f,1025,NULL,0)==0);
    const struct bench_view *v=pc_bench_read();
    if(!strcmp(argv[2],"set")) {
        CHECK(argc==5);
        ostc4_observation before=v->result;
        CHECK(pc_bench_set_gf_pair(atoi(argv[3]),atoi(argv[4]))==0);
        CHECK(!memcmp(before.nitrogen_bar,v->result.nitrogen_bar,sizeof(before.nitrogen_bar)));
        CHECK(v->seconds==0 && v->history_count==1);
        CHECK(v->storage->preference_saved);
    }
    if(!strcmp(argv[2],"dives")) {
        CHECK(pc_bench_set_gf_pair(20,60)==0);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){90,1.01325f})==0);
        CHECK(pc_bench_recording_finish()==0);
        CHECK(v->storage->log_count==1 && !v->storage->unsaved_logs);
        CHECK(pc_bench_set_gf_pair(70,70)==0);
        pc_bench_recording_arm();
        CHECK(pc_bench_accept_sample(&(ostc4_sample){120,1.31325f})==0);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){150,1.01325f})==0);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){450,1.01325f})==0);
        CHECK(v->storage->log_count==2 && !v->storage->unsaved_logs);
        CHECK(pc_bench_set_gf_pair(1,100)==0);
    }
    if(!strcmp(argv[2],"load-preserves")) {
        CHECK(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0);
        ostc4_observation before=v->result;
        struct recorded_dive recording=v->recording;
        size_t history=v->history_count;
        CHECK(pc_bench_storage_configure(argv[1])==0);
        CHECK(!memcmp(&before,&v->result,sizeof(before)));
        CHECK(!memcmp(&recording,&v->recording,sizeof(recording)));
        CHECK(v->history_count==history && v->seconds==60 && v->gf_locked);
    }
    if(!strcmp(argv[2],"legacy")) {
        CHECK(v->storage->log_error && v->storage->log_count==2);
        CHECK(v->preferred_gf.low==1 && v->preferred_gf.high==1);
    }
    if(!strcmp(argv[2],"invalid")) {
        CHECK(v->storage->preference_error && !v->storage->preference_loaded);
        CHECK(!v->storage->preference_saved);
        CHECK(v->preferred_gf.low==30 && v->preferred_gf.high==70);
        CHECK(v->seconds==0 && v->history_count==1 && !v->gf_locked);
    }
    if(!strcmp(argv[2],"fail")) {
        CHECK(argc==4);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0);
        CHECK(pc_bench_accept_sample(&(ostc4_sample){90,1.01325f})==0);
        ostc4_observation before=v->result;
        size_t count=v->storage->log_count;
        CHECK(rename(argv[1],argv[3])==0);
        FILE *block=fopen(argv[1],"w");CHECK(block && fclose(block)==0);
        CHECK(pc_bench_recording_finish()==0);
        CHECK(v->storage->unsaved_logs==1 && v->storage->log_error);
        CHECK(v->storage->log_count==count);
        CHECK(!memcmp(before.nitrogen_bar,v->result.nitrogen_bar,sizeof(before.nitrogen_bar)));
        CHECK(pc_bench_set_gf_pair(1,1)==0);
        CHECK(!v->storage->preference_saved && v->storage->preference_error);
        CHECK(v->storage->saved_preference.low==100 && v->storage->saved_preference.high==100);
        CHECK(v->preferred_gf.low==1 && v->preferred_gf.high==1);
        CHECK(v->seconds==90 && v->history_count==3);
        CHECK(!memcmp(before.nitrogen_bar,v->result.nitrogen_bar,sizeof(before.nitrogen_bar)));
        CHECK(unlink(argv[1])==0 && rename(argv[3],argv[1])==0);
    }
    if(!strcmp(argv[2],"logs")) {
        for(size_t i=0;i<v->storage->log_count;++i) {
            struct pc_bench_log record;CHECK(pc_bench_log_read(i,&record)==0);
            printf("LOG %llu %u %u %u %u %d %s\n",record.id,
                record.active_gf.low,record.active_gf.high,record.started_seconds,
                record.completed_seconds,record.automatic_completion,record.policy);
        }
    }
    printf("%u %u %d %d %zu %d\n",v->preferred_gf.low,v->preferred_gf.high,
        v->storage->preference_loaded,v->storage->preference_saved,v->storage->log_count,
        v->result.tissues);
    return 0;
}
