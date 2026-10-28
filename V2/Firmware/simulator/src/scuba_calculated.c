/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scuba_calculated.h"
#include <math.h>
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
    m->calculated=true;m->scenario=SCUBA_MANUAL;m->speed=1;
    m->calculated_state=state(r);
    if(!isfinite(depth)||depth<0||depth>61||!isfinite(r->ceiling_meters)||
        r->ceiling_meters<0||r->ceiling_meters>160||r->ndl_seconds<0||r->tts_seconds<0)
        m->calculated_state=SCUBA_CALCULATED_INVALID;
    if(m->calculated_state==SCUBA_CALCULATED_AVAILABLE||m->calculated_state==SCUBA_CALCULATED_LIMITED) {
        uint16_t stop=0;uint32_t remaining=0;
        for(int i=OSTC4_STOPS-1;i>=0;--i) if(r->stop_seconds[i]) {
            stop=(i+1)*30;remaining=(uint32_t)r->stop_seconds[i]*1000;break;
        }
        if(stop!=m->calculated_stop_dm)m->calculated_stop_total_ms=remaining;
        if(remaining>m->calculated_stop_total_ms)m->calculated_stop_total_ms=remaining;
        m->calculated_stop_dm=stop;m->calculated_stop_remaining_ms=remaining;
        m->calculated_ceiling_dm=(uint16_t)ceilf(r->ceiling_meters*10);
        m->calculated_ceiling_violation=depth<r->ceiling_meters;
        /* Stop-zone geometry follows the same millimetre sensor resolution as
         * measured depth; this avoids binary32 pressure conversion flicker at
         * an exact3m stop. Ceiling violation still compares raw depths. */
        long mm=lroundf(depth*1000);
        m->calculated_stop_zone=stop && mm>=stop*100 && mm<=(stop+30)*100;
        m->calculated_tts_s=r->tts_seconds;m->ndl_s=r->ndl_seconds;
    }
    /* Sensor-resolution millimetres first, then the established decimal-depth
     * display truncation. Raw depth controls the ceiling and stop boundaries. */
    uint32_t mm=isfinite(depth)&&depth>=0&&depth<=61?(uint32_t)lroundf(depth*1000):m->depth_dm*100u;
    scuba_model_update_mm(m,(uint64_t)seconds*1000,mm,temperature,battery);
}
