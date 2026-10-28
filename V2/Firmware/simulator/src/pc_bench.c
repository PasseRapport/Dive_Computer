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
/* PC evidence storage; never part of the portable engine or target allocation. */
static ostc4_sample actual_samples[PC_BENCH_HISTORY];
static size_t next_sample;
static unsigned char checkpoint[OSTC4_CHECKPOINT_BYTES];
static size_t checkpoint_history;
static bool checkpoint_saved;
static uint64_t pending_ms;
static void (*presentation_observer)(const struct bench_view *);
void pc_bench_set_observer(void (*observer)(const struct bench_view *))
{
    presentation_observer=observer;
}
static void notify_observer(void)
{
    if (presentation_observer) presentation_observer(&view);
}
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
    view.history=history; view.profile=graph; view.exposure=actual_samples; view.history_count=1;
    history[0]=(struct bench_point){0,0,false};
    actual_samples[0]=(ostc4_sample){0,surface};
    pending_ms=0; next_sample=1; checkpoint_saved=false;
    if (count) {
        memcpy(samples,profile,count*sizeof(*profile));
        view.duration=samples[count-1].timestamp_seconds;
        for (size_t i=0;i<count;++i) graph[i]=(struct bench_point){samples[i].timestamp_seconds,
            pressure_to_depth(samples[i].absolute_pressure_bar),
            i && samples[i].timestamp_seconds-samples[i-1].timestamp_seconds>3600};
    }
    ostc4_exposure_read(&view.result);
    notify_observer();
    return 0;
}
int pc_bench_checkpoint(void)
{
    if(!view.history_count || view.error ||
        ostc4_exposure_checkpoint(checkpoint,sizeof(checkpoint))!=OSTC4_OK)return -1;
    checkpoint_saved=true;checkpoint_history=view.history_count;
    view.recovery="Checkpoint saved (PC memory)";return 0;
}
ostc4_restart_status pc_bench_restart(enum pc_restart_mode mode)
{
    unsigned char bytes[OSTC4_CHECKPOINT_BYTES];memcpy(bytes,checkpoint,sizeof(bytes));
    ostc4_recovery recovery={mode!=PC_RESTART_CLOCK,view.seconds,86400,NULL,0};
    if(checkpoint_saved && view.history_count>=checkpoint_history && !view.gap) {
        recovery.history=actual_samples+checkpoint_history;
        recovery.count=view.history_count-checkpoint_history;
    }
    if(mode==PC_RESTART_GAP || view.gap) {recovery.history=NULL;recovery.count=1;}
    if(mode==PC_RESTART_STALE)recovery.max_age_seconds=0;
    if(mode==PC_RESTART_CORRUPT)bytes[50]^=1;
    if(mode==PC_RESTART_INCOMPATIBLE) {
        /* Synthetic valid-CRC schema fault. CRC delta for flipping byte 4 bit 0
         * of a 289-byte message, independently generated with Python zlib. */
        bytes[4]^=1;
        const uint32_t delta=UINT32_C(0x7cccaa8f);
        for(unsigned i=0;i<4;++i)bytes[sizeof(bytes)-4+i]^=(unsigned char)(delta>>(8*i));
    }
    ostc4_restart_status status=ostc4_exposure_restore(checkpoint_saved?bytes:NULL,
        checkpoint_saved?sizeof(bytes):0,&recovery);
    view.recovery=ostc4_restart_status_name(status);
    view.paused=true;view.replay=false;view.automatic=false;view.ended=false;
    view.rate_m_min=0;pending_ms=0;view.duration=view.seconds;
    ostc4_exposure_read(&view.result);
    if(status==OSTC4_RESTORED) {
        view.error=NULL;view.gap=false;
        view.depth_m=pressure_to_depth(view.result.absolute_pressure_bar);
    } else {
        view.error=view.recovery;view.gap=true;
        if(view.history_count)history[view.history_count-1].gap=true;
    }
    notify_observer();return status;
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
    view.replay=false; view.automatic=false; view.ended=false; view.depth_m=depth; view.rate_m_min=rate;
    pending_ms=0;
}
int pc_bench_auto(bool enabled)
{
    if (!view.history_count || view.error) return -1;
    view.automatic=enabled;
    view.replay=false; view.ended=false;
    view.depth_m=pressure_to_depth(view.result.absolute_pressure_bar);
    view.rate_m_min=0; view.duration=view.seconds;
    pending_ms=0;
    return 0;
}
static float automatic_depth(void)
{
    const ostc4_observation *r=&view.result;
    view.rate_m_min=0;
    if (r->tissues!=OSTC4_AVAILABLE || r->ceiling!=OSTC4_AVAILABLE ||
        r->stops!=OSTC4_AVAILABLE || r->tts!=OSTC4_AVAILABLE ||
        !isfinite(r->ceiling_meters) || r->ceiling_meters<0 || r->tts_seconds<0)
        return view.depth_m;
    float target=0;
    for (int i=OSTC4_STOPS-1;i>=0;--i) {
        if (r->stop_seconds[i]) {
            target=pressure_to_depth(view.surface_bar+0.3f*(i+1));
            break;
        }
    }
    /* A violated stop never commands an automatic descent or further ascent. */
    if (view.depth_m<target) return view.depth_m;
    float next=fmaxf(target,view.depth_m-9.0f/60.0f);
    view.rate_m_min=(next-view.depth_m)*60;
    return next;
}
static void missing(const char *reason)
{
    ostc4_exposure_missing();
    ostc4_exposure_read(&view.result);
    view.gap=true; view.error=reason; view.rate_m_min=0;
    history[view.history_count-1].gap=true;
    notify_observer();
}
void pc_bench_mark_missing(void)
{
    if(!view.history_count)return;
    missing("Unknown exposure reported: fresh fixture required");
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
            view.depth_m=view.automatic ? automatic_depth() :
                fmaxf(0,fminf(60,view.depth_m+view.rate_m_min/60));
            /* Engine permits a maximum +6 bar, independent of water density. */
            view.depth_m=fminf(view.depth_m,pressure_to_depth(view.surface_bar+6));
            pressure=depth_to_pressure(view.depth_m);
        }
        if (ostc4_exposure_advance(&(ostc4_sample){seconds,pressure})!=OSTC4_OK) {
            missing("Exposure rejected: fresh fixture required"); break;
        }
        view.seconds=seconds;
        ostc4_exposure_read(&view.result);
        actual_samples[view.history_count]=(ostc4_sample){seconds,pressure};
        history[view.history_count++]=(struct bench_point){seconds,view.depth_m,false};
        if (!view.replay) view.duration=seconds;
        if (view.replay && seconds==view.duration) { view.ended=true; pending_ms=0; }
        notify_observer();
        if (view.ended) break;
    }
}
const struct bench_view *pc_bench_read(void) { return &view; }
void pc_bench_plot(struct bench_point point, uint32_t duration, float max_depth, float *x, float *y)
{
    *x=duration ? (float)point.seconds/duration : 0;
    *y=max_depth>0 ? point.depth_m/max_depth : 0;
}
