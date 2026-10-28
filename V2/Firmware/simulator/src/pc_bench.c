/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_checkpoint_file.h"
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
static bool checkpoint_saved, checkpoint_eligible;
static struct recorded_dive checkpoint_recording;
static ostc4_gf_pair checkpoint_preferred;
static void recording_sample(uint32_t seconds,float pressure);
static int accept_sample(const ostc4_sample *sample);
static void invalidate_checkpoint_configuration(void)
{
    if(checkpoint_saved) {
        checkpoint_eligible=false;
        view.recovery="Checkpoint configuration history changed; save again";
    }
}
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
int pc_bench_storage_configure(const char *directory)
{
    int result=pc_storage_configure(directory);
    view.storage=pc_storage_read();
    notify_observer();return result;
}
int pc_bench_log_read(size_t index,struct pc_bench_log *record)
{
    return pc_storage_log_read(index,record);
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
    uint16_t start_depth=view.recording.start_depth_dm;
    uint32_t generation=view.generation+1;
    memset(&view,0,sizeof(view));
    view.recording=(struct recorded_dive){.armed=true,
        .start_depth_dm=start_depth?start_depth:20,.session_id=pc_storage_new_session_id()};
    view.generation=generation;view.gf_policy=OSTC4_CONFIGURABLE_GF_POLICY;
    view.storage=pc_storage_read();
    view.preferred_gf=view.active_gf=view.storage->preference_saved?
        view.storage->saved_preference:(ostc4_gf_pair){30,70};
    ostc4_exposure_set_gf_pair(view.preferred_gf.low,view.preferred_gf.high);
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
int pc_bench_set_gf_pair(int low,int high)
{
    if(!view.history_count || view.error || view.gf_locked ||
        ostc4_exposure_set_gf_pair(low,high)!=OSTC4_OK)return -1;
    invalidate_checkpoint_configuration();
    view.preferred_gf=view.active_gf=(ostc4_gf_pair){(uint8_t)low,(uint8_t)high};
    ostc4_exposure_read(&view.result);
    pc_storage_save_preference(view.preferred_gf);
    notify_observer();return 0;
}
int pc_bench_gf_edit_begin(void)
{
    if(!view.history_count || view.error || view.gf_locked)return -1;
    view.draft_gf=view.preferred_gf;view.gf_editing=true;
    notify_observer();return 0;
}
int pc_bench_gf_edit_adjust(bool high,int steps)
{
    if(!view.gf_editing || view.gf_locked)return -1;
    int64_t value=(high?view.draft_gf.high:view.draft_gf.low)+(int64_t)steps;
    uint8_t endpoint=(uint8_t)(value<1?1:value>100?100:value);
    if(high)view.draft_gf.high=endpoint;else view.draft_gf.low=endpoint;
    notify_observer();return 0;
}
int pc_bench_gf_edit_commit(void)
{
    if(!view.gf_editing || pc_bench_set_gf_pair(view.draft_gf.low,view.draft_gf.high))return -1;
    view.gf_editing=false;notify_observer();return 0;
}
void pc_bench_gf_edit_cancel(void)
{
    view.gf_editing=false;view.draft_gf=view.preferred_gf;notify_observer();
}
static void sync_gf_lock(void)
{
    view.gf_locked=view.recording.active;
    /* Recovered active pair remains independent of next-dive preference. */
    ostc4_exposure_set_gf_lock(view.gf_locked);
}
int pc_bench_checkpoint(void)
{
    if(!view.history_count || view.error ||
        ostc4_exposure_checkpoint(checkpoint,sizeof(checkpoint))!=OSTC4_OK)return -1;
    checkpoint_saved=checkpoint_eligible=true;checkpoint_history=view.history_count;
    checkpoint_recording=view.recording;checkpoint_preferred=view.preferred_gf;
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
        bytes[4]^=1;
        uint32_t crc=UINT32_MAX;
        for(size_t i=0;i<sizeof(bytes)-4;++i) {
            crc^=bytes[i];
            for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
        }
        crc=~crc;
        for(unsigned i=0;i<4;++i)bytes[sizeof(bytes)-4+i]=(unsigned char)(crc>>(8*i));
    }
    ostc4_restart_status status;
    if(checkpoint_saved && !checkpoint_eligible) {
        ostc4_exposure_missing();status=OSTC4_RESTART_CONFIGURATION_UNKNOWN;
    } else status=ostc4_exposure_restore(checkpoint_saved?bytes:NULL,
        checkpoint_saved?sizeof(bytes):0,&recovery);
    if(status==OSTC4_RESTORED) {
        ostc4_recovery seed={true,actual_samples[checkpoint_history-1].timestamp_seconds,0,NULL,0};
        ostc4_exposure_restore(bytes,sizeof(bytes),&seed);
        view.recording=checkpoint_recording;view.preferred_gf=checkpoint_preferred;
        ostc4_exposure_read(&view.result);view.active_gf=view.result.gf_pair;
        view.seconds=seed.restart_seconds;view.history_count=checkpoint_history;
        for(size_t n=0;n<recovery.count;++n) {
            ostc4_sample sample=recovery.history[n];
            if(accept_sample(&sample)) {status=OSTC4_RESTART_EXPOSURE_REJECTED;break;}
        }
        sync_gf_lock();
    }
    view.gf_editing=false;view.draft_gf=view.preferred_gf;
    if(status==OSTC4_RESTORED)view.data_gap=(struct exposure_gap){0};
    else if(mode==PC_RESTART_CLOCK)view.data_gap=(struct exposure_gap){true,false,false,0,0};
    else if(!view.data_gap.present && mode==PC_RESTART_GAP) {
        bool known=mode==PC_RESTART_GAP && checkpoint_saved;
        uint32_t start=known?actual_samples[checkpoint_history-1].timestamp_seconds:0;
        view.data_gap=(struct exposure_gap){true,known,false,start,known?view.seconds-start:0};
    }
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
int pc_bench_start_depth(uint16_t decimetres)
{
    if (!view.history_count || decimetres<5 ||
        decimetres>100 || decimetres%5) return -1;
    if(view.recording.start_depth_dm!=decimetres)invalidate_checkpoint_configuration();
    view.recording.start_depth_dm=decimetres;
    notify_observer();
    return 0;
}
void pc_bench_recording_arm(void)
{
    if (!view.history_count || view.recording.armed) return;
    invalidate_checkpoint_configuration();view.recording.armed=true;
    notify_observer();
}
static void save_completed_log(bool automatic)
{
    /* A saved session identity plus its recording ordinal is stable across
     * recovery, while identical profiles in new synthetic sessions stay distinct. */
    uint64_t identity=view.recording.session_id?view.recording.session_id ^ ((uint64_t)view.recording.dives_started*UINT64_C(0x9e3779b97f4a7c15)):0;
    if(!identity && view.recording.session_id)identity=view.recording.session_id;
    struct pc_bench_log record={.recording_id=identity,.started_seconds=view.recording.started_seconds,
        .completed_seconds=view.recording.completed_seconds,
        .dive_seconds=view.recording.dive_seconds,.active_gf=view.active_gf,
        .automatic_completion=automatic};
    /* Numerical identity belongs to the captured pair; later preferences never
     * replace an existing immutable log. */
    strncpy(record.policy,view.gf_policy,sizeof(record.policy)-1);
    pc_storage_reconcile_log(&record);
}
int pc_bench_recording_finish(void)
{
    long depth_mm=lroundf((view.result.absolute_pressure_bar-view.surface_bar)*10000);
    if (!view.history_count || view.error || depth_mm>=500) return -1;
    invalidate_checkpoint_configuration();
    if(view.recording.active) {
        view.recording.active=false;
        view.recording.completed_seconds=view.seconds;
        ++view.recording.dives_completed;
        save_completed_log(false);
    }
    view.recording.armed=false;
    sync_gf_lock();ostc4_exposure_read(&view.result);
    notify_observer();
    return 0;
}
static void recording_sample(uint32_t seconds,float pressure)
{
    struct recorded_dive *r=&view.recording;
    uint32_t depth_mm=(uint32_t)lroundf(fmaxf(0,(pressure-view.surface_bar)*10000));
    if (!r->armed) return;
    if (!r->active && depth_mm/100>=r->start_depth_dm) {
        r->active=true;r->started_seconds=seconds;++r->dives_started;
        r->dive_seconds=r->surface_seconds=0;
        return;
    }
    if (!r->active) {
        if(r->dives_completed && depth_mm<500)++r->surface_seconds;
        return;
    }
    if(depth_mm>=500) {++r->dive_seconds;r->surface_seconds=0;}
    else if(++r->surface_seconds>=300) {
        r->active=false;r->completed_seconds=seconds;++r->dives_completed;
    }
}
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
static void missing(const char *reason,bool known_start)
{
    ostc4_exposure_missing();
    ostc4_exposure_read(&view.result);
    view.gap=true; view.error=reason; view.rate_m_min=0;view.automatic=false;view.ended=false;
    if(!view.data_gap.present)
        view.data_gap=(struct exposure_gap){true,known_start,true,view.seconds,0};
    history[view.history_count-1].gap=true;
    notify_observer();
}
static int accept_sample(const ostc4_sample *sample)
{
    struct recorded_dive previous_recording=view.recording;
    bool previous_lock=view.gf_locked;
    uint32_t elapsed=sample->timestamp_seconds-view.seconds;
    float previous_pressure=view.result.absolute_pressure_bar;
    /* Recording follows accepted linear exposure at integer-second resolution;
     * physiology/planner run only once at the supplied segment's cadence. */
    for(uint32_t i=1;i<=elapsed;++i) {
        float pressure=previous_pressure+(sample->absolute_pressure_bar-previous_pressure)*
            ((float)i/elapsed);
        if(i==elapsed)pressure=sample->absolute_pressure_bar;
        recording_sample(view.seconds+i,pressure);
    }
    if(view.recording.dives_started!=previous_recording.dives_started) {
        view.active_gf=view.preferred_gf;
        if(view.result.gf_pair.low!=view.active_gf.low || view.result.gf_pair.high!=view.active_gf.high)
            ostc4_exposure_set_gf_pair(view.active_gf.low,view.active_gf.high);
    }
    sync_gf_lock(); /* before the calculation which accepts a dive-start input */
    if(ostc4_exposure_advance(sample)!=OSTC4_OK) {
        view.recording=previous_recording;view.gf_locked=previous_lock;
        ostc4_exposure_set_gf_lock(previous_lock);
        return -1;
    }
    if(view.recording.dives_started!=previous_recording.dives_started) {
        view.gf_editing=false;view.draft_gf=view.preferred_gf;
    }
    if(view.recording.dives_completed!=previous_recording.dives_completed)
        save_completed_log(true);
    view.seconds=sample->timestamp_seconds;view.elapsed_seconds=view.seconds;
    view.depth_m=pressure_to_depth(sample->absolute_pressure_bar);
    ostc4_exposure_read(&view.result);
    actual_samples[view.history_count]=*sample;
    history[view.history_count++]=(struct bench_point){view.seconds,view.depth_m,false};
    return 0;
}
int pc_bench_accept_sample(const ostc4_sample *sample)
{
    if(!view.history_count || view.error || !sample || !isfinite(sample->absolute_pressure_bar) ||
        sample->absolute_pressure_bar<view.surface_bar || sample->absolute_pressure_bar>view.surface_bar+6 ||
        sample->timestamp_seconds<=view.seconds || sample->timestamp_seconds-view.seconds>3600 ||
        view.history_count==PC_BENCH_HISTORY)return -1;
    if(accept_sample(sample)) {missing("Exposure rejected: fresh fixture required",false);return -1;}
    view.replay=view.automatic=view.ended=false;pending_ms=0;view.rate_m_min=0;
    view.duration=view.seconds;notify_observer();return 0;
}
void pc_bench_mark_missing(void)
{
    if(!view.history_count)return;
    view.data_gap=(struct exposure_gap){true,false,true,view.seconds,0};
    missing("Unknown exposure reported: fresh fixture required",false);
}
void pc_bench_sensor_loss(void)
{
    if(!view.history_count || view.error)return;
    pending_ms=0;
    missing("Sensor signal lost: exposure unknown",true);
}
void pc_bench_poll(uint32_t elapsed_ms)
{
    if (!view.speed||view.paused||view.ended) return;
    if(view.error) {
        if(!view.data_gap.ongoing)return;
        pending_ms+=(uint64_t)elapsed_ms*view.speed;
        uint64_t seconds=pending_ms/1000;
        pending_ms%=1000;
        if(seconds>UINT32_MAX-view.elapsed_seconds)seconds=UINT32_MAX-view.elapsed_seconds;
        if(seconds) {
            view.elapsed_seconds+=(uint32_t)seconds;
            if(view.data_gap.time_known)
                view.data_gap.duration_seconds=view.elapsed_seconds-view.data_gap.start_seconds;
            notify_observer();
        }
        return;
    }
    pending_ms+=(uint64_t)elapsed_ms*view.speed;
    while (pending_ms>=1000) {
        pending_ms-=1000;
        if (view.history_count==PC_BENCH_HISTORY) { missing("History full: fresh fixture required",false); break; }
        uint32_t seconds=view.seconds+1;
        float pressure;
        if (view.replay) {
            const ostc4_sample a=samples[next_sample-1], b=samples[next_sample];
            if (b.timestamp_seconds-a.timestamp_seconds>3600) {
                /* The timestamps bound the missing interval, even though its
                 * pressure cannot be integrated. Advance only the adapter clock. */
                view.elapsed_seconds=b.timestamp_seconds;
                view.data_gap=(struct exposure_gap){true,true,false,a.timestamp_seconds,
                    b.timestamp_seconds-a.timestamp_seconds};
                pending_ms=0;
                missing("Missing exposure: replay gap; fresh fixture required",false); break;
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
        if(accept_sample(&(ostc4_sample){seconds,pressure})) {
            missing("Exposure rejected: fresh fixture required",false);break;
        }
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

void pc_bench_checkpoint_notice(const char *message)
{
    view.recovery=message;notify_observer();
}
int pc_bench_checkpoint_export(struct pc_bench_checkpoint_state *state)
{
    if(!state || !view.history_count || view.error || !view.recording.session_id ||
       ostc4_exposure_checkpoint(state->core,sizeof(state->core))!=OSTC4_OK)return -1;
    state->recording=view.recording;state->preferred=view.preferred_gf;
    state->density=view.density_kg_m3;return 0;
}
ostc4_restart_status pc_bench_checkpoint_reject(ostc4_restart_status status)
{
    ostc4_observation retained=view.result;
    if(ostc4_exposure_missing()!=OSTC4_OK)ostc4_exposure_restore(NULL,0,NULL);
    ostc4_exposure_read(&view.result);
    if(view.history_count) {
        /* A second failed import cannot erase previously accepted diagnostics,
         * even when the already-invalid core cannot export a backup checkpoint. */
        retained.tissues=retained.ndl=retained.tts=retained.ceiling=retained.stops=OSTC4_INVALID;
        retained.gf_low_ceiling=retained.gf_high_ceiling=OSTC4_INVALID;
        view.result=retained;
    }
    view.error=view.recovery=ostc4_restart_status_name(status);
    view.gap=true;view.paused=true;view.replay=view.automatic=false;
    view.gf_editing=false;pending_ms=0;
    view.data_gap=(struct exposure_gap){true,false,false,0,0};
    if(view.history_count)history[view.history_count-1].gap=true;
    notify_observer();return status;
}
ostc4_restart_status pc_bench_checkpoint_import(const struct pc_bench_checkpoint_state *state,
    const ostc4_recovery *evidence,bool legacy_ambiguous)
{
    unsigned char prior[OSTC4_CHECKPOINT_BYTES];
    bool prior_valid=ostc4_exposure_checkpoint(prior,sizeof(prior))==OSTC4_OK;
    ostc4_restart_status status=ostc4_exposure_restore(state->core,sizeof(state->core),evidence);
    if(status!=OSTC4_RESTORED) {
        /* Preserve prior accepted diagnostic values while latching INVALID. */
        if(prior_valid) {
            ostc4_recovery old={true,view.seconds,0,NULL,0};
            ostc4_exposure_restore(prior,sizeof(prior),&old);
        }
        return pc_bench_checkpoint_reject(status);
    }
    ostc4_gf_pair preferred=view.history_count?view.preferred_gf:state->preferred;
    uint32_t generation=view.generation+1;
    memset(&view,0,sizeof(view));view.generation=generation;
    view.storage=pc_storage_read();
    view.history=history;view.profile=graph;view.exposure=actual_samples;
    view.gf_policy=OSTC4_CONFIGURABLE_GF_POLICY;view.preferred_gf=preferred;
    view.density_kg_m3=state->density;view.speed=1;view.paused=true;
    view.recording=state->recording;view.active_gf=(ostc4_gf_pair){state->core[289],state->core[290]};
    /* Seed the graph at the checkpoint; no pre-checkpoint trace is invented. */
    unsigned char const *b=state->core;
    uint32_t seconds=(uint32_t)b[16]|(uint32_t)b[17]<<8|(uint32_t)b[18]<<16|(uint32_t)b[19]<<24;
    uint32_t bits=(uint32_t)b[24]|(uint32_t)b[25]<<8|(uint32_t)b[26]<<16|(uint32_t)b[27]<<24;
    memcpy(&view.surface_bar,&bits,4);
    bits=(uint32_t)b[28]|(uint32_t)b[29]<<8|(uint32_t)b[30]<<16|(uint32_t)b[31]<<24;
    float pressure;memcpy(&pressure,&bits,4);
    actual_samples[0]=(ostc4_sample){seconds,pressure};
    history[0]=(struct bench_point){seconds,pressure_to_depth(pressure),false};view.history_count=1;
    /* The validation above checks the whole evidence contract without emitting
     * logs. Rewind to the checkpoint and process each exact segment through the
     * same controller-before-calculation path as uninterrupted exposure. */
    ostc4_recovery seed={true,seconds,0,NULL,0};
    ostc4_exposure_restore(state->core,sizeof(state->core),&seed);
    view.seconds=view.elapsed_seconds=seconds;ostc4_exposure_read(&view.result);
    for(size_t n=0;evidence && n<evidence->count;++n) {
        if(accept_sample(&evidence->history[n])) {
            return pc_bench_checkpoint_reject(OSTC4_RESTART_EXPOSURE_REJECTED);
        }
    }
    seconds=view.seconds;pressure=view.result.absolute_pressure_bar;
    view.seconds=view.elapsed_seconds=view.duration=seconds;
    view.depth_m=pressure_to_depth(pressure);view.gf_locked=legacy_ambiguous || view.recording.active;
    ostc4_exposure_set_gf_lock(view.gf_locked);ostc4_exposure_read(&view.result);
    view.draft_gf=view.preferred_gf;checkpoint_saved=false;pending_ms=0;next_sample=1;
    if(legacy_ambiguous) {
        pc_bench_checkpoint_reject(OSTC4_RESTART_CONFIGURATION_UNKNOWN);
        view.recovery="Legacy fixed 30/70 migrated; recording lifecycle UNKNOWN; original file retained";
        view.error=view.recovery;notify_observer();return OSTC4_RESTART_CONFIGURATION_UNKNOWN;
    }
    view.recovery="Recovered disk checkpoint: complete exposure and recording lifecycle";
    notify_observer();return OSTC4_RESTORED;
}
