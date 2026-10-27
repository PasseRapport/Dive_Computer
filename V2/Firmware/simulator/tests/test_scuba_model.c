#include "scuba_model.h"
#include <assert.h>
#include <stdio.h>

static void update(struct scuba_model *s,uint64_t now,uint16_t depth)
{scuba_model_update(s,now,depth,18,87);}
static void start(struct scuba_model *s)
{scuba_model_init(s);scuba_model_arm(s,0);update(s,1000,20);assert(s->active);}
int main(void)
{
    struct scuba_model custom;
    scuba_model_init(&custom);custom.start_depth_dm=35;scuba_model_arm(&custom,0);
    update(&custom,1000,34);assert(!custom.active);
    scuba_model_scenario(&custom,SCUBA_MANUAL,1000);assert(custom.start_depth_dm==35);
    update(&custom,2000,35);assert(custom.active);
    struct scuba_model s;
    scuba_model_init(&s);scuba_model_arm(&s,0);
    update(&s,1000,19);assert(!s.active);
    update(&s,2000,20);assert(s.active);
    update(&s,12000,100);assert(s.peak_dm==100&&s.dive_ms==10000);
    update(&s,13000,4);assert(s.active&&s.surface_ms==1000);
    update(&s,14000,5);assert(s.active&&s.surface_ms==0);
    update(&s,15000,0);update(&s,313999,0);assert(s.active);
    update(&s,314000,0);assert(!s.active&&s.summary.valid&&s.summary.max_dm==100);
    assert(s.summary.duration_s==11&&s.summary.missed_stop);
    uint32_t surface=s.surface_ms;update(&s,314500,0);assert(s.surface_ms==surface+500);
    update(&s,315000,20);assert(s.active&&s.peak_dm==20);
    s.depth_alarm_dm=30;s.time_alarm_s=2;
    update(&s,317000,40);assert((s.alerts&(SCUBA_DEPTH|SCUBA_TIME))==(SCUBA_DEPTH|SCUBA_TIME));
    scuba_model_acknowledge(&s);update(&s,317050,40);assert(!s.notifications);
    update(&s,318050,20);assert(!(s.acknowledged&SCUBA_DEPTH));
    update(&s,319050,40);assert(s.notifications&SCUBA_DEPTH);
    scuba_model_update(&s,320050,40,14,5);assert(s.alerts&SCUBA_BATTERY);
    assert(s.min_temperature==14);
    start(&s);update(&s,2000,200);update(&s,3000,100);
    assert(s.alerts&SCUBA_ASCENT);
    update(&s,7000,100);assert(!(s.alerts&SCUBA_ASCENT));
    start(&s);update(&s,2000,100);update(&s,3000,50);
    assert(scuba_model_in_stop(&s)&&s.stop_ms==1000);
    update(&s,13000,50);assert(s.stop_ms==11000);
    update(&s,23000,20);assert(s.stop_ms==11000);
    update(&s,192000,50);assert(s.stop_ms==180000);
    update(&s,193000,0);scuba_model_finish(&s);
    assert(s.summary.valid&&!s.summary.missed_stop&&!s.active);
    start(&s);update(&s,2000,100);update(&s,3000,50);
    scuba_model_finish(&s);assert(s.active); /* No finish underwater. */
    update(&s,3050,0);scuba_model_finish(&s);assert(!s.active);
    update(&s,3100,20);assert(s.active&&!(s.alerts&SCUBA_ASCENT));
    start(&s);s.speed=60;
    update(&s,2000,50);assert(s.dive_ms==60000);
    scuba_model_scenario(&s,SCUBA_DECO,2000);
    update(&s,2001,0);assert(!s.active);
    for(uint64_t now=2050;now<18000;now+=50)update(&s,now,0);
    assert(s.summary.valid&&!s.active);
    assert(!s.summary.missed_stop);
    /* Explicit ceiling violation and paused decompression timer. */
    start(&s);s.deco_started=true;
    update(&s,2000,50);assert(s.alerts&SCUBA_CEILING);assert(s.stop_ms==0);
    update(&s,3000,60);assert(!(s.alerts&SCUBA_CEILING)&&s.stop_ms==1000);
    update(&s,122000,60);assert(s.deco_stage==1&&scuba_model_ceiling(&s)==30);
    update(&s,212000,30);assert(s.deco_done&&!scuba_model_ceiling(&s));
    start(&s);update(&s,2000,600);assert(s.alerts&SCUBA_OXYGEN);
    scuba_model_acknowledge(&s);update(&s,3000,600);assert(!(s.notifications&SCUBA_OXYGEN));
    update(&s,4000,100);assert(!(s.alerts&SCUBA_OXYGEN));
    puts("SCUBA MODEL PASS: 2 m start, surface threshold/delay, redive, summaries, alarms/acknowledgement, ascent, stop pauses, staged deco and time acceleration");
}
