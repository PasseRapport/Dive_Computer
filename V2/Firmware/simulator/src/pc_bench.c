/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include <math.h>
#include <string.h>

const ostc4_sample pc_bench_two_dives[8] = {
    {0,1.01325f},{120,4.01325f},{1920,4.01325f},{2100,1.01325f},
    {3600,1.01325f},{3720,3.01325f},{4920,3.01325f},{5040,1.01325f}
};
static struct bench_view view;
static struct bench_point history[PC_BENCH_HISTORY], graph[PC_BENCH_PROFILE];
static ostc4_sample samples[PC_BENCH_PROFILE];
static size_t next_sample;
static uint64_t pending_ms;
static float depth_to_pressure(float depth)
{
    return view.surface_bar + depth * view.density_kg_m3 * 9.80665f / 100000.0f;
}
static float pressure_to_depth(float pressure)
{
    return (pressure-view.surface_bar)*100000.0f/(view.density_kg_m3*9.80665f);
}
int pc_bench_fresh(float surface, float density, const ostc4_sample *profile, size_t count)
{
    /* Validate before replacing any live fixture. */
    if (!isfinite(surface)||surface<0.8f||surface>1.2f||!isfinite(density)||density<990||density>1040||
        count>PC_BENCH_PROFILE||(count && (!profile||count<2))) return -1;
    for (size_t i=0;i<count;++i) {
        if (!isfinite(profile[i].absolute_pressure_bar)||profile[i].absolute_pressure_bar<surface||
            profile[i].absolute_pressure_bar>surface+6||profile[i].timestamp_seconds>=PC_BENCH_HISTORY||
            (i && profile[i].timestamp_seconds<=profile[i-1].timestamp_seconds)) return -1;
    }
    if (count && (profile[0].timestamp_seconds!=0||profile[0].absolute_pressure_bar!=surface)) return -1;
    if (ostc4_exposure_initialize(&(ostc4_initial){0,surface})!=OSTC4_OK) return -1;
    memset(&view,0,sizeof(view));
    view.surface_bar=surface; view.density_kg_m3=density; view.speed=1;
    view.replay=count!=0; view.profile_count=count;
    view.history=history; view.profile=graph; view.history_count=1;
    history[0]=(struct bench_point){0,0,false};
    pending_ms=0; next_sample=1;
    if (count) {
        memcpy(samples,profile,count*sizeof(*profile));
        view.duration=samples[count-1].timestamp_seconds;
        for (size_t i=0;i<count;++i) graph[i]=(struct bench_point){samples[i].timestamp_seconds,
            pressure_to_depth(samples[i].absolute_pressure_bar),
            i && samples[i].timestamp_seconds-samples[i-1].timestamp_seconds>3600};
    }
    ostc4_exposure_read(&view.result);
    return 0;
}
void pc_bench_pause(bool paused) { view.paused=paused; }
void pc_bench_speed(unsigned speed)
{
    if (speed==1||speed==10||speed==60) view.speed=speed;
}
void pc_bench_manual(float depth, float rate)
{
    if (!isfinite(depth)||depth<0||depth>60||depth_to_pressure(depth)>view.surface_bar+6||
        !isfinite(rate)||fabsf(rate)>60) return;
    view.replay=false; view.ended=false; view.depth_m=depth; view.rate_m_min=rate;
    pending_ms=0;
}
static void missing(const char *reason)
{
    ostc4_exposure_missing();
    ostc4_exposure_read(&view.result);
    view.gap=true; view.error=reason;
}
void pc_bench_mark_missing(void)
{
    if(!view.history_count)return;
    missing("Unknown exposure reported: fresh fixture required");
    history[view.history_count-1].gap=true;
}
void pc_bench_poll(uint32_t elapsed_ms)
{
    if (!view.speed||view.paused||view.ended||view.error) return;
    pending_ms+=(uint64_t)elapsed_ms*view.speed;
    while (pending_ms>=1000) {
        pending_ms-=1000;
        if (view.history_count==PC_BENCH_HISTORY) { missing("History full: fresh fixture required"); break; }
        uint32_t seconds=view.seconds+1;
        float pressure;
        if (view.replay) {
            const ostc4_sample a=samples[next_sample-1], b=samples[next_sample];
            if (b.timestamp_seconds-a.timestamp_seconds>3600) {
                missing("Missing exposure: replay gap; fresh fixture required"); break;
            }
            pressure=a.absolute_pressure_bar+(b.absolute_pressure_bar-a.absolute_pressure_bar)*
                ((float)(seconds-a.timestamp_seconds)/(b.timestamp_seconds-a.timestamp_seconds));
            if (seconds==b.timestamp_seconds) pressure=b.absolute_pressure_bar;
            view.depth_m=pressure_to_depth(pressure);
            if (seconds==b.timestamp_seconds) ++next_sample;
        } else {
            view.depth_m=fmaxf(0,fminf(60,view.depth_m+view.rate_m_min/60));
            /* Engine permits a maximum +6 bar, independent of water density. */
            view.depth_m=fminf(view.depth_m,pressure_to_depth(view.surface_bar+6));
            pressure=depth_to_pressure(view.depth_m);
        }
        if (ostc4_exposure_advance(&(ostc4_sample){seconds,pressure})!=OSTC4_OK) {
            missing("Exposure rejected: fresh fixture required"); break;
        }
        view.seconds=seconds;
        ostc4_exposure_read(&view.result);
        history[view.history_count++]=(struct bench_point){seconds,view.depth_m,false};
        if (!view.replay) view.duration=seconds;
        if (view.replay && seconds==view.duration) { view.ended=true; pending_ms=0; break; }
    }
}
const struct bench_view *pc_bench_read(void) { return &view; }
void pc_bench_plot(struct bench_point point, uint32_t duration, float max_depth, float *x, float *y)
{
    *x=duration ? (float)point.seconds/duration : 0;
    *y=max_depth>0 ? point.depth_m/max_depth : 0;
}
