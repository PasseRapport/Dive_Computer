/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "scuba_calculated.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static void automatic_takeover(void)
{
    CHECK(pc_bench_fresh(1.0f,1000,NULL,0)==0);
    const struct bench_view *v=pc_bench_read();
    pc_bench_manual(12.345f,0);
    pc_bench_poll(60000);
    ostc4_observation before=v->result;
    size_t count=v->history_count;
    /* Discard an unobserved depth command when re-engaging. */
    pc_bench_manual(35,0);
    CHECK(pc_bench_auto(true)==0);
    CHECK(v->automatic && !v->replay && v->seconds==60);
    CHECK(fabsf(v->depth_m-12.345f)<1e-5f);
    CHECK(memcmp(&before,&v->result,sizeof(before))==0 && v->history_count==count);
    pc_bench_poll(1000);
    /* Declared ascent policy is 9m/min, or 0.15m in one exposure second. */
    CHECK(fabsf(v->depth_m-12.195f)<1e-5f);
    CHECK(v->history_count==count+1 && v->history[count].seconds==61);
    CHECK(v->exposure[count].timestamp_seconds==v->result.timestamp_seconds);
    CHECK(v->exposure[count].absolute_pressure_bar==v->result.absolute_pressure_bar);
    CHECK(fabsf(v->history[count].depth_m-v->depth_m)<1e-6f);
    CHECK(pc_bench_auto(false)==0 && !v->automatic && v->rate_m_min==0);
    pc_bench_poll(1000);
    CHECK(fabsf(v->depth_m-12.195f)<1e-5f);
    CHECK(pc_bench_auto(true)==0);
    pc_bench_manual(v->depth_m,6);
    CHECK(!v->automatic);
    pc_bench_poll(1000);
    CHECK(fabsf(v->depth_m-12.295f)<1e-5f);
}
static void automatic_holds_unavailable_and_invalid(void)
{
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
    const struct bench_view *v=pc_bench_read();
    pc_bench_manual(12,0); pc_bench_poll(1000);
    CHECK(v->result.tts==OSTC4_UNAVAILABLE);
    CHECK(pc_bench_auto(true)==0);
    pc_bench_poll(59000);
    CHECK(v->seconds==60 && fabsf(v->depth_m-12)<1e-5f);
    CHECK(v->result.tts==OSTC4_AVAILABLE);
    pc_bench_poll(1000);
    CHECK(fabsf(v->depth_m-11.85f)<1e-5f);
    pc_bench_mark_missing();
    CHECK(v->result.tts==OSTC4_INVALID && v->history[v->history_count-1].gap);
    uint32_t seconds=v->seconds;
    float depth=v->depth_m;
    pc_bench_poll(60000);
    CHECK(v->seconds==seconds && v->depth_m==depth && v->rate_m_min==0);
    CHECK(pc_bench_auto(true)==-1);
}
static void automatic_acceleration_preserves_exposure(void)
{
    ostc4_observation normal={0};
    for (unsigned speed=1;speed<=60;speed=speed==1?10:60) {
        CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
        pc_bench_manual(12,0); pc_bench_poll(60000);
        CHECK(pc_bench_auto(true)==0);
        pc_bench_speed(speed);
        const struct bench_view *v=pc_bench_read();
        while (v->seconds<180) pc_bench_poll(1000);
        CHECK(v->seconds==180 && v->depth_m==0 && v->history_count==181);
        if (speed==1) normal=v->result;
        else CHECK(memcmp(&normal,&v->result,sizeof(normal))==0);
        if (speed==60) break;
    }
}
static void automatic_stop_progression_and_violation(void)
{
    CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
    const struct bench_view *v=pc_bench_read();
    pc_bench_speed(60); pc_bench_poll(32000); pc_bench_speed(1);
    CHECK(v->seconds==1920 && v->result.stops==OSTC4_AVAILABLE);
    CHECK(v->result.stop_seconds[0]>0 && v->result.stop_seconds[3]>0);
    CHECK(pc_bench_auto(true)==0);
    bool held_stop=false, released_stop=false;
    for (unsigned second=0;second<3000 && v->depth_m>0;second++) {
        float before=v->depth_m;
        pc_bench_poll(1000);
        CHECK(!v->error && v->automatic);
        CHECK(v->depth_m<=before && before-v->depth_m<0.15001f);
        CHECK(fabsf(v->history[v->history_count-1].depth_m-v->depth_m)<1e-6f);
        /* +0.3 bar in 1025 kg/m3 seawater is 2.98453576m, not 3m. */
        if (fabsf(v->depth_m-2.98453576f)<1e-5f && v->result.stop_seconds[0]) held_stop=true;
        if (held_stop && v->depth_m<2.9f) released_stop=true;
    }
    CHECK(held_stop && released_stop && v->depth_m==0);
    uint32_t surfaced=v->seconds;
    pc_bench_speed(60); pc_bench_poll(25000);
    CHECK(v->seconds==surfaced+1500 && v->depth_m==0);
    CHECK(!v->recording.active && v->recording.dives_completed==1);
    CHECK(v->result.nitrogen_bar[5]>0.9f);
    pc_bench_manual(20,0); pc_bench_poll(20000);
    CHECK(v->seconds==surfaced+2700 && !v->automatic);
    CHECK(v->recording.active && v->recording.dives_started==2);
    CHECK(pc_bench_auto(true)==0);
    pc_bench_poll(1000);
    CHECK(v->depth_m<20 && v->history_count==v->seconds+1);

    CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
    pc_bench_speed(60); pc_bench_poll(32000); pc_bench_speed(1);
    pc_bench_manual(2,0); pc_bench_poll(1000);
    CHECK(v->result.stops==OSTC4_AVAILABLE && v->result.stop_seconds[0]>0);
    CHECK(pc_bench_auto(true)==0);
    pc_bench_poll(10000);
    CHECK(fabsf(v->depth_m-2)<1e-5f && v->rate_m_min==0);
}
static void recording_start_threshold(void)
{
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
    const struct bench_view *v=pc_bench_read();
    CHECK(v->recording.armed && !v->recording.active && v->recording.start_depth_dm==20);
    pc_bench_manual(1.9f,0);pc_bench_poll(1000);
    CHECK(!v->recording.active);
    CHECK(pc_bench_start_depth(30)==0);
    pc_bench_manual(2.5f,0);pc_bench_poll(1000);
    CHECK(!v->recording.active);
    pc_bench_manual(3.1f,0);pc_bench_poll(1000);
    CHECK(v->recording.active && v->recording.started_seconds==3 &&
          v->recording.dive_seconds==0 && v->recording.dives_started==1);
    CHECK(v->seconds==3 && v->result.timestamp_seconds==3 && v->history_count==4);
    pc_bench_poll(1000);
    CHECK(v->recording.dive_seconds==1);
    /* Existing start-depth settings remain editable for the next recording. */
    CHECK(pc_bench_start_depth(20)==0);
    CHECK(v->recording.active && v->recording.dives_started==1 && v->recording.dive_seconds==1);
}
static struct scuba_model recorded_watch;
static void recording_observer(const struct bench_view *v)
{
    scuba_calculated_apply_recorded(&recorded_watch,&v->recording,&v->result,v->seconds,
        (v->result.absolute_pressure_bar-v->surface_bar)*10,18,87);
    CHECK(recorded_watch.active==v->recording.active);
    CHECK(recorded_watch.armed==v->recording.armed);
    CHECK(recorded_watch.dive_ms==(uint64_t)v->recording.dive_seconds*1000);
    CHECK(recorded_watch.surface_ms==v->recording.surface_seconds*1000);
}
static void recording_surface_completion_and_second_dive(void)
{
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
    CHECK(pc_bench_start_depth(20)==0);
    scuba_model_init(&recorded_watch);
    pc_bench_set_observer(recording_observer);
    recording_observer(pc_bench_read());
    const struct bench_view *v=pc_bench_read();
    pc_bench_manual(3,0);pc_bench_poll(11000);
    CHECK(v->recording.active && v->recording.dive_seconds==10);
    CHECK(pc_bench_recording_finish()==-1);
    /* Hydrostatic .51m becomes .500m on the watch: exactly .5 is underwater. */
    pc_bench_manual(.51f,0);pc_bench_poll(1000);
    CHECK(v->recording.surface_seconds==0 && v->recording.dive_seconds==11);
    pc_bench_manual(.50f,0);pc_bench_poll(299000);
    CHECK(v->recording.active && v->recording.surface_seconds==299);
    pc_bench_manual(3,0);pc_bench_poll(1000);
    CHECK(v->recording.active && v->recording.dives_started==1 &&
          v->recording.surface_seconds==0 && v->recording.dive_seconds==12);
    pc_bench_manual(0,0);pc_bench_poll(1000);
    ostc4_observation before=v->result;
    size_t history_count=v->history_count;
    CHECK(pc_bench_recording_finish()==0);
    CHECK(!v->recording.active && !v->recording.armed && v->recording.dives_completed==1);
    CHECK(recorded_watch.summary.valid && recorded_watch.summary.duration_s==12);
    CHECK(before.gf_locked && !v->result.gf_locked);
    before.gf_locked=false; /* Completion changes lock metadata only. */
    CHECK(memcmp(&before,&v->result,sizeof(before))==0 && v->history_count==history_count);
    /* Manual completion keeps both elapsed surface time and tissues moving. */
    uint32_t surface_before=v->recording.surface_seconds;
    pc_bench_poll(60000);
    CHECK(v->recording.surface_seconds==surface_before+60);
    CHECK(recorded_watch.surface_ms==(surface_before+60)*1000);
    CHECK(v->result.desaturation==OSTC4_AVAILABLE);
    before=v->result;history_count=v->history_count;
    pc_bench_recording_arm();
    CHECK(memcmp(&before,&v->result,sizeof(before))==0 && v->history_count==history_count);
    pc_bench_manual(3,0);pc_bench_poll(1000);
    CHECK(v->recording.active && v->recording.dives_started==2 && v->recording.dive_seconds==0);
    pc_bench_poll(10000);pc_bench_manual(0,0);pc_bench_poll(299000);
    CHECK(v->recording.active && v->recording.surface_seconds==299);
    pc_bench_poll(1000);
    CHECK(!v->recording.active && v->recording.armed && v->recording.surface_seconds==300 &&
          v->recording.dives_completed==2 && recorded_watch.summary.duration_s==10);
    pc_bench_poll(1000);
    CHECK(v->recording.surface_seconds==301);
    pc_bench_set_observer(NULL);
}
static void desaturation_summary_follows_repetitive_exposure(void)
{
    const ostc4_sample profile[]={{0,1.01325f},{120,3.01325f},{1320,3.01325f},
        {1500,1.01325f},{3300,1.01325f},{3420,3.01325f},{4620,3.01325f},
        {4800,1.01325f},{6600,1.01325f}};
    ostc4_observation normal={0};
    for(unsigned speed=1;speed<=60;speed=speed==1?10:60) {
        CHECK(!pc_bench_fresh(1.01325f,1025,profile,9));
        scuba_model_init(&recorded_watch);pc_bench_set_observer(recording_observer);
        recording_observer(pc_bench_read());pc_bench_speed(speed);
        ostc4_observation fresh=pc_bench_read()->result;
        const struct bench_view *v=pc_bench_read();
        /* A replay is sampled on its existing one-second clock at every speed. */
        while(v->seconds<1800)pc_bench_poll(1000);
        CHECK(!v->recording.active && v->recording.dives_completed==1);
        CHECK(recorded_watch.summary.valid && recorded_watch.summary.desaturation_valid);
        CHECK(recorded_watch.summary.desaturation_minutes==(unsigned)v->result.desaturation_minutes);
        CHECK(v->result.desaturation_minutes>0);
        struct scuba_summary summary=recorded_watch.summary;
        ostc4_observation before=v->result,read;
        pc_bench_pause(true);pc_bench_poll(60000);
        for(unsigned n=0;n<4;++n) {
            CHECK(ostc4_exposure_read(&read)==OSTC4_OK);
            CHECK(!memcmp(&before,&read,sizeof(read)));
            recording_observer(v);
        }
        CHECK(!memcmp(&summary,&recorded_watch.summary,sizeof(summary)));
        CHECK(!memcmp(&before,&v->result,sizeof(before)));
        pc_bench_pause(false);
        while(v->seconds<3300)pc_bench_poll(1000);
        CHECK(v->result.nitrogen_bar[15]>fresh.nitrogen_bar[15]);
        while(!v->ended && !v->error)pc_bench_poll(1000);
        CHECK(!v->error && v->seconds==6600 && v->recording.dives_completed==2);
        CHECK(recorded_watch.summary.valid && !recorded_watch.active);
        CHECK(recorded_watch.summary.desaturation_valid);
        CHECK(recorded_watch.summary.desaturation_minutes==(unsigned)v->result.desaturation_minutes);
        CHECK(v->result.nitrogen_bar[15]>fresh.nitrogen_bar[15]);
        if(speed==1)normal=v->result;
        else CHECK(!memcmp(&normal,&v->result,sizeof(normal)));
        uint32_t seconds=v->seconds;
        pc_bench_mark_missing();
        CHECK(v->seconds==seconds && !recorded_watch.summary.desaturation_valid);
        pc_bench_set_observer(NULL);
        if(speed==60)break;
    }
}
static void desaturation_is_available_at_surface_equilibrium(void)
{
    CHECK(pc_bench_fresh(1.0f,1000,NULL,0)==0);
    const ostc4_observation *r=&pc_bench_read()->result;
    CHECK(r->desaturation==OSTC4_AVAILABLE);
    CHECK(r->desaturation_minutes==0);
    pc_bench_mark_missing();
    CHECK(pc_bench_read()->result.desaturation==OSTC4_INVALID);
}
int main(void)
{
    desaturation_is_available_at_surface_equilibrium();
    desaturation_summary_follows_repetitive_exposure();
    recording_surface_completion_and_second_dive();
    recording_start_threshold();
    automatic_takeover();
    automatic_holds_unavailable_and_invalid();
    automatic_acceleration_preserves_exposure();
    automatic_stop_progression_and_violation();
    /* Worked hydrostatic example: 1000 kg/m3, g=9.80665, 12.345m:
     * 1.0 + 1.2106314675 bar. No display rounding in the input. */
    CHECK(pc_bench_fresh(1.0f,1000,NULL,0)==0);
    pc_bench_manual(12.345f,0);
    pc_bench_poll(1000);
    const struct bench_view *v=pc_bench_read();
    CHECK(fabsf(v->result.absolute_pressure_bar-2.2106314675f)<1e-6f);
    CHECK(v->seconds==1 && v->history_count==2);
    CHECK(v->exposure[0].timestamp_seconds==0 && v->exposure[0].absolute_pressure_bar==1.0f);
    CHECK(v->exposure[1].timestamp_seconds==1 &&
          v->exposure[1].absolute_pressure_bar==v->result.absolute_pressure_bar);
    CHECK(fabsf(v->history[1].depth_m-12.345f)<1e-5f);
    pc_bench_manual(12.345f,6);
    pc_bench_poll(1000);
    CHECK(fabsf(v->depth_m-12.445f)<1e-5f);
    float x,y;
    pc_bench_plot((struct bench_point){25,15,false},100,60,&x,&y);
    CHECK(x==0.25f && y==0.25f);
    ostc4_observation normal={0};
    for (unsigned speed=1;speed<=60;speed=speed==1?10:60) {
        CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
        pc_bench_speed(speed);
        pc_bench_poll(1000);
        ostc4_observation before=v->result;
        uint32_t frozen=v->seconds;
        pc_bench_pause(true); pc_bench_poll(12345);
        CHECK(v->seconds==frozen && memcmp(&before,&v->result,sizeof(before))==0);
        pc_bench_pause(false);
        while (!v->ended && !v->error) pc_bench_poll(1000);
        if(v->error) fprintf(stderr,"at %u: %s\n",v->seconds,v->error);
        CHECK(v->ended && !v->error && v->seconds==5040);
        CHECK(v->result.timestamp_seconds==5040 && v->history_count==5041);
        CHECK(v->recording.active && v->recording.dives_started==2 && v->recording.dives_completed==1);
        CHECK(v->result.tissues==OSTC4_AVAILABLE);
        CHECK(v->result.nitrogen_bar[5]>0.9f); /* residual loading above fresh ~0.76 bar */
        if (speed==1) normal=v->result;
        else CHECK(memcmp(&normal,&v->result,sizeof(normal))==0);
        pc_bench_plot(v->history[v->history_count-1],v->duration,60,&x,&y);
        CHECK(x==1 && fabsf(y)<1e-5f);
        if (speed==60) break;
    }
    /* Surface/manual takeover are inputs, never fresh initialization. */
    CHECK(pc_bench_fresh(1.01325f,1025,pc_bench_two_dives,8)==0);
    pc_bench_speed(60); pc_bench_poll(32000);
    CHECK(v->seconds==1920);
    float loaded=v->result.nitrogen_bar[5];
    pc_bench_manual(0,0);
    CHECK(v->result.nitrogen_bar[5]==loaded && v->seconds==1920);
    pc_bench_poll(1000);
    CHECK(v->seconds==1980 && v->result.nitrogen_bar[5]>0.9f);
    CHECK(v->history_count==1981 && !v->replay);
    const ostc4_sample gap[]={{0,1.0f},{4000,2.0f}};
    CHECK(pc_bench_fresh(1,1000,gap,2)==0);
    pc_bench_poll(1000);
    CHECK(v->gap && v->error && v->result.tissues==OSTC4_INVALID);
    CHECK(v->seconds==0 && v->history_count==1 && v->profile[1].gap);
    CHECK(v->elapsed_seconds==4000 && v->data_gap.present && v->data_gap.time_known);
    CHECK(v->data_gap.start_seconds==0 && v->data_gap.duration_seconds==4000 && !v->data_gap.ongoing);
    CHECK(pc_bench_fresh(1,1000,NULL,0)==0);
    pc_bench_poll(2000);
    pc_bench_mark_missing();
    CHECK(v->gap && v->result.tissues==OSTC4_INVALID && v->history[v->history_count-1].gap);
    pc_bench_poll(1000); CHECK(v->seconds==2);
    CHECK(pc_bench_fresh(NAN,1000,NULL,0)==-1);
    return 0;
}
