#include "scuba_model.h"
#include <string.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
static uint16_t segment(uint32_t t, uint32_t begin, uint32_t end, int from, int to)
{
    if (t <= begin) return from;
    if (t >= end) return to;
    return from + (to-from)*(int32_t)(t-begin)/(int32_t)(end-begin);
}
static uint16_t scripted_depth(enum scuba_scenario scenario, uint32_t t)
{
    if (scenario == SCUBA_NO_DECO) {
        if (t < 65) return segment(t,5,65,0,80);
        if (t < 125) return 80;
        return segment(t,125,185,80,0);
    }
    if (scenario == SCUBA_SAFETY) {
        if (t < 125) return segment(t,5,125,0,200);
        if (t < 185) return 200;
        if (t < 285) return segment(t,185,285,200,50);
        if (t < 470) return 50;
        return segment(t,470,505,50,0);
    }
    if (scenario == SCUBA_DECO) {
        if (t < 125) return segment(t,5,125,0,200);
        if (t < 245) return 200;
        if (t < 345) return segment(t,245,345,200,60);
        if (t < 470) return 60;
        if (t < 490) return segment(t,470,490,60,30);
        if (t < 590) return 30;
        return segment(t,590,610,30,0);
    }
    if (t < 35) return segment(t,5,35,0,250);
    if (t < 65) return 250;
    if (t < 70) return segment(t,65,70,250,20);
    if (t < 110) return 20;
    return segment(t,110,115,20,0);
}
void scuba_model_init(struct scuba_model *s)
{
    memset(s,0,sizeof(*s));
    s->speed = 1;
    s->start_depth_dm = 20;
    s->battery = 87;
    s->ndl_s = s->ndl_budget_s = 1200;
}
void scuba_model_arm(struct scuba_model *s, uint64_t now_ms)
{
    s->armed = true;
    s->last_ms = now_ms;
}
bool scuba_model_surface(const struct scuba_model *s) { return s->depth_dm < 5; }
uint16_t scuba_model_ceiling(const struct scuba_model *s)
{
    return s->active && s->deco_started && !s->deco_done ? (s->deco_stage == 0 ? 60 : 30) : 0;
}
uint32_t scuba_model_stop_target(const struct scuba_model *s)
{
    if (scuba_model_ceiling(s)) return s->deco_stage == 0 ? 120000 : 90000;
    return s->safety_required ? 180000 : 0;
}
bool scuba_model_in_stop(const struct scuba_model *s)
{
    uint16_t ceiling = scuba_model_ceiling(s);
    return s->active && (ceiling ? s->depth_dm >= ceiling && s->depth_dm <= ceiling+30 :
                          s->safety_required && s->depth_dm >= 30 && s->depth_dm <= 60);
}
uint32_t scuba_model_tts(const struct scuba_model *s)
{
    uint32_t remaining = (scuba_model_stop_target(s)-MIN(s->stop_ms,scuba_model_stop_target(s)))/1000;
    if (scuba_model_ceiling(s) && s->deco_stage == 0) remaining += 90;
    return remaining + s->depth_dm*6/10; /* Scripted 10 m/min ascent component. */
}
void scuba_model_finish(struct scuba_model *s)
{
    if (!s->active || !scuba_model_surface(s)) return;
    s->summary = (struct scuba_summary){
        .valid=true, .duration_s=s->dive_ms/1000, .max_dm=s->peak_dm,
        .average_dm=s->underwater_ms ? s->depth_integral/s->underwater_ms : 0,
        .min_temperature=s->min_temperature,
        .missed_stop=(s->safety_required && s->stop_ms < 180000) || (s->deco_started && !s->deco_done),
    };
    s->active=false;
    s->alerts=s->acknowledged=s->notifications=0;
}
void scuba_model_scenario(struct scuba_model *s, enum scuba_scenario scenario, uint64_t now_ms)
{
    /* An explicit desktop scenario selection starts a fresh demonstration;
     * settings and the last completed summary survive the reset. */
    struct scuba_summary summary=s->summary;
    uint16_t alarm=s->depth_alarm_dm, start=s->start_depth_dm;
    uint32_t time=s->time_alarm_s;
    uint8_t speed=s->speed;
    bool salt=s->salt_water, armed=s->armed;
    scuba_model_init(s);
    s->summary=summary; s->start_depth_dm=start; s->depth_alarm_dm=alarm; s->time_alarm_s=time;
    s->speed=speed; s->salt_water=salt; s->armed=armed; s->scenario=scenario;
    s->last_ms=now_ms;
}
const char *scuba_model_scenario_name(enum scuba_scenario scenario)
{
    static const char *const names[]={"Manual", "No deco", "Safety stop", "Deco", "Alerts"};
    return names[scenario < SCUBA_SCENARIO_COUNT ? scenario : 0];
}
void scuba_model_acknowledge(struct scuba_model *s)
{
    s->acknowledged |= s->alerts;
    s->notifications=0;
}
void scuba_model_update(struct scuba_model *s, uint64_t now_ms, uint16_t manual_depth_dm, int16_t temperature, uint8_t battery)
{
    scuba_model_update_mm(s,now_ms,(uint32_t)manual_depth_dm*100,temperature,battery);
}
void scuba_model_update_mm(struct scuba_model *s, uint64_t now_ms, uint32_t manual_depth_mm, int16_t temperature, uint8_t battery)
{
    uint64_t dt=now_ms >= s->last_ms ? (now_ms-s->last_ms)*s->speed : 0;
    s->last_ms=now_ms;
    if (!s->armed) return;
    s->scenario_ms += dt;
    uint32_t t=s->scenario_ms/1000;
    s->depth_dm=s->scenario == SCUBA_MANUAL ? manual_depth_mm/100 : scripted_depth(s->scenario,t);
    uint32_t depth_mm=s->scenario==SCUBA_MANUAL?manual_depth_mm:(uint32_t)s->depth_dm*100;
    s->temperature=temperature;
    s->battery=s->scenario == SCUBA_ALERTS && t >= 85 ? 5 : battery;
    if (!s->active && s->depth_dm >= s->start_depth_dm) {
        s->active=true; s->dive_ms=s->depth_integral=s->underwater_ms=0;
        s->peak_dm=s->depth_dm; s->min_temperature=temperature;
        s->stop_ms=s->surface_ms=0; s->safety_required=s->deco_started=s->deco_done=false;
        s->deco_stage=0; s->rate_ms=s->scenario_ms; s->rate_depth_mm=depth_mm;
        s->alerts=s->acknowledged=s->notifications=0;
        s->fast_until_ms=0; s->ascent_dm_min=0; s->fast_ascent_ms=0;
        dt=0;
    }
    if (!s->active) {
        if(s->summary.valid && scuba_model_surface(s))s->surface_ms=MIN((uint64_t)UINT32_MAX,s->surface_ms+dt);
        return;
    }
    if (!scuba_model_surface(s)) {
        s->dive_ms+=dt; s->underwater_ms+=dt; s->depth_integral+=s->depth_dm*dt;
        s->surface_ms=0;
        if (s->depth_dm > s->peak_dm) s->peak_dm=s->depth_dm;
        if (temperature < s->min_temperature) s->min_temperature=temperature;
    } else {
        s->surface_ms += dt;
        if (s->surface_ms >= 300000) { scuba_model_finish(s); return; }
    }
    uint32_t budget=s->scenario == SCUBA_NO_DECO ? 7200 : s->scenario == SCUBA_ALERTS ? 240 : 1200;
    uint32_t exposure=s->dive_ms/1000;
    if (s->scenario == SCUBA_DECO) { budget=150; exposure=t; }
    s->ndl_budget_s=budget;
    s->ndl_s=exposure < budget ? budget-exposure : 0;
    if (!s->ndl_s && !s->deco_started) { s->deco_started=true; s->stop_ms=0; }
    if (s->peak_dm >= 100 && !s->deco_started) s->safety_required=true;
    uint32_t target=scuba_model_stop_target(s);
    if (target && scuba_model_in_stop(s)) {
        s->stop_ms=MIN((uint64_t)target,s->stop_ms+dt);
        if (s->stop_ms == target && scuba_model_ceiling(s)) {
            if (s->deco_stage == 0) { s->deco_stage=1; s->stop_ms=0; }
            else { s->deco_done=true; s->safety_required=false; }
        }
    }
    uint64_t interval=s->scenario_ms-s->rate_ms;
    if (interval >= 1000) {
        /* Round only the final velocity, never the depth samples. */
        int64_t rate=((int64_t)s->rate_depth_mm-depth_mm)*600;
        s->ascent_dm_min=rate>=0?(rate+(int64_t)interval/2)/(int64_t)interval:
            -((-rate+(int64_t)interval/2)/(int64_t)interval);
        if (s->ascent_dm_min > 100) {
            s->fast_until_ms=s->scenario_ms+3000;
            s->fast_ascent_ms=MIN((uint64_t)UINT32_MAX,s->fast_ascent_ms+interval);
        } else s->fast_ascent_ms=0;
        s->rate_depth_mm=depth_mm; s->rate_ms=s->scenario_ms;
    }
    uint32_t alerts=0;
    if (scuba_model_ceiling(s) && s->depth_dm < scuba_model_ceiling(s)) alerts|=SCUBA_CEILING;
    if (s->scenario_ms < s->fast_until_ms) alerts|=SCUBA_ASCENT;
    if (s->battery <= 15) alerts|=SCUBA_BATTERY;
    if ((100u+s->depth_dm)*21u > 14000) alerts|=SCUBA_OXYGEN;
    if (s->deco_started && !s->deco_done) alerts|=SCUBA_DECOMPRESSION;
    if (!s->deco_started && s->ndl_s <= 300) alerts|=SCUBA_NDL;
    if (s->depth_alarm_dm && s->depth_dm >= s->depth_alarm_dm) alerts|=SCUBA_DEPTH;
    if (s->time_alarm_s && s->dive_ms/1000 >= s->time_alarm_s) alerts|=SCUBA_TIME;
    s->acknowledged &= alerts;
    s->alerts=alerts;
    /* Unacknowledged alerts stay visible even if another alert clears. */
    s->notifications &= alerts;
    s->notifications |= alerts & ~s->acknowledged;
}
