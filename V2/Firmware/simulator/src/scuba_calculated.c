/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scuba_calculated.h"
#include <math.h>
#include "water.h"
static enum scuba_calculated_state state(const ostc4_observation *r)
{
    ostc4_availability fields[]={r->tissues,r->ndl,r->tts,r->ceiling,r->stops};
    bool missing=false,limited=false;
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        if(fields[i]<OSTC4_UNAVAILABLE||fields[i]>OSTC4_INVALID)return SCUBA_CALCULATED_INVALID;
        if(fields[i]==OSTC4_INVALID)return SCUBA_CALCULATED_INVALID;
        if(fields[i]==OSTC4_UNAVAILABLE)missing=true;
        if(fields[i]==OSTC4_LIMITED)limited=true;
    }
    if(missing)return SCUBA_CALCULATED_UNAVAILABLE;
    return limited?SCUBA_CALCULATED_LIMITED:SCUBA_CALCULATED_AVAILABLE;
}
void scuba_calculated_apply(struct scuba_model *m,const ostc4_observation *r,
    uint32_t seconds,float depth,int16_t temperature,uint8_t battery)
{
    bool previously_valid=m->calculated && (m->calculated_state==SCUBA_CALCULATED_AVAILABLE ||
        m->calculated_state==SCUBA_CALCULATED_LIMITED);
    m->calculated=true;m->scenario=SCUBA_MANUAL;m->speed=1;
    m->calculated_state=state(r);
    if(isfinite(r->absolute_pressure_bar) && r->absolute_pressure_bar>0)
        m->calculated_ppo2_bar=r->absolute_pressure_bar*0.21f;
    float density=water_density(m->salt_water);
    float maximum_depth=water_depth(6.0f,0,density);
    float ceiling=water_depth(r->ceiling_meters/10.0f,0,density);
    m->calculated_gf_low_ceiling_m=water_depth(r->gf_low_ceiling_meters/10.0f,0,density);
    m->calculated_gf_high_ceiling_m=water_depth(r->gf_high_ceiling_meters/10.0f,0,density);
    m->calculated_gf_low_ceiling_valid=r->tissues==OSTC4_AVAILABLE && r->gf_low_ceiling==OSTC4_AVAILABLE;
    m->calculated_gf_high_ceiling_valid=r->tissues==OSTC4_AVAILABLE && r->gf_high_ceiling==OSTC4_AVAILABLE;
    if(!isfinite(depth)||depth<0||depth>maximum_depth||!isfinite(r->ceiling_meters)||
        r->ceiling_meters<0||r->ceiling_meters>160||r->ndl_seconds<0||r->tts_seconds<0)
        m->calculated_state=SCUBA_CALCULATED_INVALID;
    if(m->calculated_state==SCUBA_CALCULATED_AVAILABLE||m->calculated_state==SCUBA_CALCULATED_LIMITED) {
        uint16_t stop=0;uint32_t remaining=0;unsigned slot=0;
        float lower=0,upper=0;
        for(int i=OSTC4_STOPS-1;i>=0;--i) if(r->stop_seconds[i]) {
            slot=i+1;lower=water_depth(0.3f*slot,0,density);
            upper=water_depth(0.3f*(slot+1),0,density);
            stop=(uint16_t)lroundf(lower*10);remaining=(uint32_t)r->stop_seconds[i]*1000;break;
        }
        long mm=lroundf(depth*1000);
        bool in_stop=slot && mm>=lroundf(lower*1000) && mm<=lroundf(upper*1000);
        uint32_t display_remaining=remaining;
        if(previously_valid && slot==m->calculated_stop_slot && remaining &&
           remaining==m->calculated_stop_raw_ms && in_stop && m->calculated_stop_zone) {
            uint64_t now=(uint64_t)seconds*1000;
            uint64_t elapsed=now>=m->last_ms?now-m->last_ms:0;
            /* Interpolate the planner's ten-second slots on the exposure clock.
             * Only the planner can clear a stop: retain at least one second. */
            display_remaining=elapsed<m->calculated_stop_remaining_ms ?
                m->calculated_stop_remaining_ms-(uint32_t)elapsed:1000;
            if(display_remaining<1000)display_remaining=1000;
        }
        if(slot!=m->calculated_stop_slot)m->calculated_stop_total_ms=remaining;
        if(remaining>m->calculated_stop_total_ms)m->calculated_stop_total_ms=remaining;
        m->calculated_stop_slot=slot;
        m->calculated_stop_lower_m=lower;m->calculated_stop_upper_m=upper;
        m->calculated_stop_dm=stop;m->calculated_stop_remaining_ms=display_remaining;
        m->calculated_stop_raw_ms=remaining;
        m->calculated_ceiling_dm=(uint16_t)ceilf(ceiling*10);
        m->calculated_ceiling_violation=depth<ceiling;
        /* Stop-zone geometry follows the same millimetre sensor resolution as
         * measured depth; this avoids binary32 pressure conversion flicker at
         * an exact pressure stop. Ceiling violation still compares raw depths. */
        m->calculated_stop_zone=in_stop;
        m->calculated_stop_shallow=slot && mm<lroundf(lower*1000);
        m->calculated_tts_s=r->tts_seconds;m->ndl_s=r->ndl_seconds;
    }
    /* Sensor-resolution millimetres first, then the established decimal-depth
     * display truncation. Raw depth controls the ceiling and stop boundaries. */
    uint32_t mm=isfinite(depth)&&depth>=0&&depth<=maximum_depth?(uint32_t)lroundf(depth*1000):m->depth_dm*100u;
    scuba_model_update_mm(m,(uint64_t)seconds*1000,mm,temperature,battery);
}

void scuba_calculated_apply_recorded(struct scuba_model *m,const struct recorded_dive *recording,
    const ostc4_observation *r,uint32_t seconds,float depth,int16_t temperature,uint8_t battery)
{
    m->recording_external=true;m->recorded_active=recording->active;
    /* Let an active presentation finish its summary before disarming. */
    m->armed=recording->armed || m->active;
    m->start_depth_dm=recording->start_depth_dm;
    scuba_calculated_apply(m,r,seconds,depth,temperature,battery);
    m->armed=recording->armed;
    m->dive_ms=(uint64_t)recording->dive_seconds*1000;
    m->surface_ms=recording->surface_seconds*1000;
}
