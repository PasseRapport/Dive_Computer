/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Experimental comparison adapter; upstream calculations live in upstream/. */
#include "ostc4_baseline.h"
#include "decom.h"
#include "buehlmann.h"
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

/* Upstream copies 4*16 bytes and its baseline uses binary32/binary64. */
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "OSTC4 baseline requires IEEE binary32 float");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "OSTC4 baseline requires IEEE binary64 double");
extern float gGF_low_depth_bar;
static SLifeData life;
static SDiveSettings settings;
static ostc4_observation current;
static bool initialized;

ostc4_status ostc4_initialize(const ostc4_initial *initial)
{
    if (!initial || !isfinite(initial->surface_pressure_bar) ||
        initial->surface_pressure_bar < 0.8f || initial->surface_pressure_bar > 1.2f)
        return OSTC4_BAD_INPUT;
    memset(&life, 0, sizeof(life));
    memset(&settings, 0, sizeof(settings));
    memset(&current, 0, sizeof(current));
    life.pressure_surface_bar = initial->surface_pressure_bar;
    life.pressure_ambient_bar = initial->surface_pressure_bar;
    decom_reset_with_ambientmbar(initial->surface_pressure_bar, &life);
    life.actualGas.nitrogen_percentage = 79;
    life.actualGas.AppliedDiveMode = DIVEMODE_OC;
    settings.diveMode = DIVEMODE_OC;
    settings.gf_low = 30;
    settings.gf_high = 70;
    settings.ascentRate_meterperminute = 9;
    settings.last_stop_depth_bar = 0.3f;
    settings.input_second_to_last_stop_depth_bar = 0.6f;
    settings.input_next_stop_increment_depth_bar = 0.3f;
    settings.decogaslist[0] = life.actualGas;
    gGF_low_depth_bar = 0; /* Match zero-initialized upstream process state. */
    current.gf_pair = (ostc4_gf_pair){30,70};
    current.timestamp_seconds = initial->timestamp_seconds;
    current.absolute_pressure_bar = initial->surface_pressure_bar;
    memcpy(current.nitrogen_bar, life.tissue_nitrogen_bar, sizeof(current.nitrogen_bar));
    memcpy(current.helium_bar, life.tissue_helium_bar, sizeof(current.helium_bar));
    current.tissues = OSTC4_AVAILABLE;
    initialized = true;
    return OSTC4_OK;
}

/* Air-only analytical fixed-GF endpoints. This consumes no planner scratch:
 * reads and predictions cannot change the active interpolation anchor. */
static void endpoint_ceilings(ostc4_observation *r, float surface)
{
    r->gf_low_ceiling_meters = r->gf_high_ceiling_meters = 0;
    r->gf_low_ceiling = r->gf_high_ceiling = r->tissues;
    if (r->tissues != OSTC4_AVAILABLE) return;
    if (!isfinite(surface) || surface < 0.8f || surface > 1.2f ||
        r->gf_pair.low < 1 || r->gf_pair.high > 100 || r->gf_pair.low > r->gf_pair.high) {
        r->gf_low_ceiling = r->gf_high_ceiling = OSTC4_INVALID;
        return;
    }
    double pressure[2] = {surface, surface};
    const unsigned factors[2] = {r->gf_pair.low, r->gf_pair.high};
    for (int i = 0; i < OSTC4_TISSUES; ++i) {
        if (!isfinite(r->nitrogen_bar[i]) || r->nitrogen_bar[i] < 0 || r->helium_bar[i] != 0) {
            r->gf_low_ceiling = r->gf_high_ceiling = OSTC4_INVALID;
            return;
        }
        for (unsigned endpoint = 0; endpoint < 2; ++endpoint) {
            const double gf = factors[endpoint] / 100.0;
            const double required = (r->nitrogen_bar[i] - gf * buehlmann_N2_a[i]) /
                (1.0 + gf * (1.0 / buehlmann_N2_b[i] - 1.0));
            if (required > pressure[endpoint]) pressure[endpoint] = required;
        }
    }
    r->gf_low_ceiling_meters = (float)((pressure[0] - surface) * 10.0);
    r->gf_high_ceiling_meters = (float)((pressure[1] - surface) * 10.0);
}

ostc4_status ostc4_read(ostc4_observation *observation)
{
    if (!observation)
        return OSTC4_BAD_INPUT;
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    *observation = current;
    endpoint_ceilings(observation, life.pressure_surface_bar);
    return OSTC4_OK;
}

static ostc4_status calculate(SLifeData *next, SDiveSettings *next_settings,
    uint32_t timestamp, ostc4_observation *output)
{
    SDecoinfo result = {0};
    ostc4_observation observed = {0};
    const float old_gf_depth = gGF_low_depth_bar;
    buehlmann_calc_deco(next, next_settings, &result);
    /* Upstream ceiling consumes scratch GF left by the immediately prior planner. */
    buehlmann_ceiling_calculator(next, &result);
    for (int i = 0; i < OSTC4_TISSUES; ++i) {
        if (!isfinite(next->tissue_nitrogen_bar[i]) || next->tissue_nitrogen_bar[i] < 0 ||
            !isfinite(next->tissue_helium_bar[i]) || next->tissue_helium_bar[i] < 0) {
            gGF_low_depth_bar = old_gf_depth;
            return OSTC4_NUMERICAL_FAILURE;
        }
    }
    if (!isfinite(result.output_ceiling_meter) || !isfinite(gGF_low_depth_bar) ||
        !isfinite(next_settings->internal__pressure_first_stop_ambient_bar_as_upper_limit_for_gf_low_otherwise_zero)) {
        gGF_low_depth_bar = old_gf_depth;
        return OSTC4_NUMERICAL_FAILURE;
    }
    observed.gf_pair = (ostc4_gf_pair){next_settings->gf_low,next_settings->gf_high};
    observed.gf_locked = current.gf_locked;
    observed.timestamp_seconds = timestamp;
    observed.dive_seconds = next->dive_time_seconds_without_surface_time;
    observed.absolute_pressure_bar = next->pressure_ambient_bar;
    memcpy(observed.nitrogen_bar, next->tissue_nitrogen_bar, sizeof(observed.nitrogen_bar));
    memcpy(observed.helium_bar, next->tissue_helium_bar, sizeof(observed.helium_bar));
    observed.gf_anchor_ambient_bar = next_settings->internal__pressure_first_stop_ambient_bar_as_upper_limit_for_gf_low_otherwise_zero;
    observed.gf_low_depth_bar = gGF_low_depth_bar;
    observed.ndl_seconds = result.output_ndl_seconds;
    observed.tts_seconds = result.output_time_to_surface_seconds;
    observed.ceiling_meters = result.output_ceiling_meter;
    memcpy(observed.stop_seconds, result.output_stop_length_seconds, sizeof(observed.stop_seconds));
    observed.tissues = OSTC4_AVAILABLE;
    if (observed.dive_seconds >= 60 && next->pressure_ambient_bar > next->pressure_surface_bar &&
        (observed.ndl_seconds > 0 || observed.tts_seconds > 0)) {
        observed.ndl = observed.ndl_seconds >= 14400 ? OSTC4_LIMITED : OSTC4_AVAILABLE;
        observed.tts = observed.tts_seconds >= 59940 ? OSTC4_LIMITED : OSTC4_AVAILABLE;
        observed.stops = observed.ceiling = observed.tts;
        for (int i = 0; i < OSTC4_STOPS; ++i)
            if (observed.stop_seconds[i] >= 59940)
                observed.stops = OSTC4_LIMITED;
    }
    endpoint_ceilings(&observed, next->pressure_surface_bar);
    *output = observed;
    return OSTC4_OK;
}

ostc4_status ostc4_advance(const ostc4_sample *sample)
{
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    if (!sample || !isfinite(sample->absolute_pressure_bar) ||
        sample->absolute_pressure_bar < life.pressure_surface_bar ||
        sample->absolute_pressure_bar > life.pressure_surface_bar + 6.0f)
        return OSTC4_BAD_INPUT;
    if (sample->timestamp_seconds <= current.timestamp_seconds ||
        sample->timestamp_seconds - current.timestamp_seconds > 3600)
        return OSTC4_BAD_TIME;
    const uint32_t elapsed = sample->timestamp_seconds - current.timestamp_seconds;
    SLifeData next = life;
    SDiveSettings next_settings = settings;
    ostc4_observation observed = {0};
    next.pressure_ambient_bar = sample->absolute_pressure_bar;
    /* This baseline excludes wholly surface intervals from the upstream dive
     * counter; transitions include their whole duration. No recording threshold. */
    if (next.pressure_ambient_bar > life.pressure_surface_bar ||
        life.pressure_ambient_bar > life.pressure_surface_bar)
        next.dive_time_seconds_without_surface_time += elapsed;
    const float depth = (next.pressure_ambient_bar - next.pressure_surface_bar) * 10.0f;
    if (depth > next.max_depth_meter)
        next.max_depth_meter = depth;
    if (next.pressure_ambient_bar == life.pressure_ambient_bar)
        decom_tissues_exposure((int)elapsed, &next);
    else
        decom_tissues_exposure_stage_schreiner((int)elapsed, &next.actualGas,
            life.pressure_ambient_bar, next.pressure_ambient_bar,
            next.tissue_nitrogen_bar, next.tissue_helium_bar);
    ostc4_status status=calculate(&next,&next_settings,sample->timestamp_seconds,&observed);
    if(status!=OSTC4_OK)return status;
    life = next;
    settings = next_settings;
    current = observed;
    return OSTC4_OK;
}

const char *ostc4_availability_name(ostc4_availability availability)
{
    switch (availability) {
    case OSTC4_UNAVAILABLE: return "unavailable";
    case OSTC4_AVAILABLE: return "available";
    case OSTC4_LIMITED: return "limited";
    case OSTC4_INVALID: return "invalid";
    }
    return "invalid";
}

#ifdef OSTC4_CONTINUOUS_EXPOSURE
#include "ostc4_exposure.h"
#include "ostc4_scratch.h"
static bool exposure_incomplete;

static void invalidate_exposure(void)
{
    exposure_incomplete = true;
    current.tissues = current.ndl = current.tts = current.ceiling = current.stops = OSTC4_INVALID;
}

ostc4_status ostc4_exposure_initialize(const ostc4_initial *initial)
{
    ostc4_status status = ostc4_initialize(initial);
    if (status == OSTC4_OK)
        exposure_incomplete = false;
    else if (initialized)
        invalidate_exposure();
    return status;
}

ostc4_status ostc4_exposure_read(ostc4_observation *observation)
{
    return ostc4_read(observation);
}

ostc4_status ostc4_exposure_missing(void)
{
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    invalidate_exposure();
    return OSTC4_OK;
}

ostc4_status ostc4_exposure_advance(const ostc4_sample *sample)
{
    if (exposure_incomplete)
        return OSTC4_BAD_INPUT;
    ostc4_status status = ostc4_advance(sample);
    if (status != OSTC4_OK && initialized)
        invalidate_exposure();
    return status;
}

ostc4_status ostc4_exposure_set_gf_pair(int low,int high)
{
    if(!initialized)return OSTC4_NOT_INITIALIZED;
    if(exposure_incomplete || current.gf_locked || low<1 || high>100 || low>high)
        return OSTC4_BAD_INPUT;
    SDiveSettings next_settings=settings;
    next_settings.gf_low=(uint8_t)low;next_settings.gf_high=(uint8_t)high;
    SLifeData next=life;
    ostc4_observation observed={0};
    ostc4_scratch scratch;ostc4_scratch_save(&scratch);
    ostc4_status status=calculate(&next,&next_settings,current.timestamp_seconds,&observed);
    if(status!=OSTC4_OK) {ostc4_scratch_restore(&scratch);return status;}
    settings=next_settings;current=observed;
    return OSTC4_OK;
}
ostc4_status ostc4_exposure_set_gf_lock(bool locked)
{
    if(!initialized)return OSTC4_NOT_INITIALIZED;
    current.gf_locked=locked;
    return OSTC4_OK;
}

ostc4_status ostc4_exposure_predict(const ostc4_sample *future, ostc4_observation *prediction)
{
    if (!prediction)
        return OSTC4_BAD_INPUT;
    memset(prediction, 0, sizeof(*prediction));
    if (!initialized)
        return OSTC4_NOT_INITIALIZED;
    if (exposure_incomplete) {
        prediction->tissues = prediction->ndl = prediction->tts =
            prediction->ceiling = prediction->stops = prediction->gf_low_ceiling =
            prediction->gf_high_ceiling = OSTC4_INVALID;
        return OSTC4_BAD_INPUT;
    }
    const SLifeData saved_life = life;
    const SDiveSettings saved_settings = settings;
    const ostc4_observation saved_current = current;
    ostc4_scratch scratch;
    ostc4_scratch_save(&scratch);
    ostc4_status status = ostc4_advance(future);
    if (status == OSTC4_OK)
        *prediction = current;
    life = saved_life;
    settings = saved_settings;
    current = saved_current;
    ostc4_scratch_restore(&scratch);
    return status;
}
/* Checkpoint v1 layout and compatibility contract are in CHECKPOINT.txt.
 * Byte codec avoids padding, alignment, endianness and upstream struct ABI.
 */
static uint32_t checkpoint_crc(const unsigned char *bytes, size_t size)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<size;++i) {
        crc^=bytes[i];
        for(unsigned bit=0;bit<8;++bit)
            crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
    }
    return ~crc;
}
static uint32_t checkpoint_policy(void)
{
    static const unsigned char policy[]=OSTC4_REVISION "\n" OSTC4_POLICY "\n"
        OSTC4_CORRECTION_SET "\n" OSTC4_PLANNER_CORRECTION_SET "\n" OSTC4_CONFIGURABLE_GF_POLICY
        "\nieee-binary32-binary64-no-fast-math-no-contract-checkpoint-v2";
    return checkpoint_crc(policy,sizeof(policy)-1);
}
static void put_u32(unsigned char **p,uint32_t value)
{
    for(unsigned i=0;i<4;++i) *(*p)++=(unsigned char)(value>>(8*i));
}
static uint32_t get_u32(const unsigned char **p)
{
    uint32_t value=0;
    for(unsigned i=0;i<4;++i)value|=(uint32_t)*(*p)++<<(8*i);
    return value;
}
static void put_float(unsigned char **p,float value)
{
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));put_u32(p,bits);
}
static float get_float(const unsigned char **p)
{
    uint32_t bits=get_u32(p);float value;memcpy(&value,&bits,sizeof(value));return value;
}
ostc4_status ostc4_exposure_checkpoint(void *buffer,size_t size)
{
    if(!initialized)return OSTC4_NOT_INITIALIZED;
    if(exposure_incomplete || !buffer || size!=OSTC4_CHECKPOINT_BYTES)return OSTC4_BAD_INPUT;
    unsigned char bytes[OSTC4_CHECKPOINT_BYTES],*p=bytes;
    put_u32(&p,UINT32_C(0x31435845)); /* EXC1 */
    put_u32(&p,2);put_u32(&p,checkpoint_policy());put_u32(&p,OSTC4_CHECKPOINT_BYTES);
    put_u32(&p,current.timestamp_seconds);put_u32(&p,current.dive_seconds);
    put_float(&p,life.pressure_surface_bar);put_float(&p,current.absolute_pressure_bar);
    put_float(&p,life.max_depth_meter);put_float(&p,current.gf_anchor_ambient_bar);
    put_float(&p,current.gf_low_depth_bar);put_float(&p,current.ceiling_meters);
    for(int i=0;i<OSTC4_TISSUES;++i)put_float(&p,current.nitrogen_bar[i]);
    for(int i=0;i<OSTC4_TISSUES;++i)put_float(&p,current.helium_bar[i]);
    put_u32(&p,(uint32_t)current.ndl_seconds);put_u32(&p,(uint32_t)current.tts_seconds);
    for(int i=0;i<OSTC4_STOPS;++i) {
        *p++=(unsigned char)current.stop_seconds[i];
        *p++=(unsigned char)(current.stop_seconds[i]>>8);
    }
    *p++=(unsigned char)current.tissues;*p++=(unsigned char)current.ndl;
    *p++=(unsigned char)current.tts;*p++=(unsigned char)current.ceiling;*p++=(unsigned char)current.stops;
    *p++=current.gf_pair.low;*p++=current.gf_pair.high;*p++=current.gf_locked;
    put_u32(&p,checkpoint_crc(bytes,sizeof(bytes)-4));
    memcpy(buffer,bytes,sizeof(bytes));return OSTC4_OK;
}
static bool checkpoint_number(float value,float low,float high)
{
    return isfinite(value) && value>=low && value<=high;
}
static bool checkpoint_observation_valid(const ostc4_observation *r,float surface,float max_depth)
{
    if(r->dive_seconds>r->timestamp_seconds || !checkpoint_number(surface,0.8f,1.2f) ||
       !checkpoint_number(r->absolute_pressure_bar,surface,surface+6) ||
       !checkpoint_number(max_depth,0,60.00001f) ||
       !checkpoint_number(r->gf_anchor_ambient_bar,0,surface+15) ||
       !checkpoint_number(r->gf_low_depth_bar,0,15) ||
       !checkpoint_number(r->ceiling_meters,0,150) ||
       r->ndl_seconds<0 || r->ndl_seconds>14400 || r->tts_seconds<0 || r->tts_seconds>59940 ||
       r->gf_pair.low<1 || r->gf_pair.high>100 || r->gf_pair.low>r->gf_pair.high ||
       r->tissues!=OSTC4_AVAILABLE || r->ndl>OSTC4_LIMITED || r->tts>OSTC4_LIMITED ||
       r->ceiling>OSTC4_LIMITED || r->stops>OSTC4_LIMITED)return false;
    for(int i=0;i<OSTC4_TISSUES;++i)
        if(!checkpoint_number(r->nitrogen_bar[i],0,8) || r->helium_bar[i]!=0)return false;
    for(int i=0;i<OSTC4_STOPS;++i)if(r->stop_seconds[i]>59940)return false;
    return true;
}
ostc4_restart_status ostc4_exposure_restore(const void *buffer,size_t size,const ostc4_recovery *recovery)
{
    /* Rejection must be visible even before a first fixture exists. */
    memset(&current,0,sizeof(current));initialized=true;invalidate_exposure();
    if(buffer && size==293) {
        /* Fixed-30/70 v1 has no recording/lock evidence. Migration belongs to
         * the caller's explicit compatibility path, never an unlocked default. */
        const unsigned char *old=buffer,*tail=old+289;
        return get_u32(&tail)==checkpoint_crc(old,289)?
            OSTC4_CHECKPOINT_INCOMPATIBLE:OSTC4_CHECKPOINT_CORRUPT;
    }
    if(!buffer || size!=OSTC4_CHECKPOINT_BYTES)return OSTC4_CHECKPOINT_CORRUPT;
    const unsigned char *bytes=buffer,*tail=bytes+size-4,*p=bytes;
    if(get_u32(&tail)!=checkpoint_crc(bytes,size-4))return OSTC4_CHECKPOINT_CORRUPT;
    if(get_u32(&p)!=UINT32_C(0x31435845) || get_u32(&p)!=2 ||
       get_u32(&p)!=checkpoint_policy() || get_u32(&p)!=OSTC4_CHECKPOINT_BYTES)
        return OSTC4_CHECKPOINT_INCOMPATIBLE;
    ostc4_observation saved={0};
    saved.timestamp_seconds=get_u32(&p);saved.dive_seconds=get_u32(&p);
    const float surface=get_float(&p);
    saved.absolute_pressure_bar=get_float(&p);
    const float max_depth=get_float(&p);
    saved.gf_anchor_ambient_bar=get_float(&p);saved.gf_low_depth_bar=get_float(&p);
    saved.ceiling_meters=get_float(&p);
    for(int i=0;i<OSTC4_TISSUES;++i)saved.nitrogen_bar[i]=get_float(&p);
    for(int i=0;i<OSTC4_TISSUES;++i)saved.helium_bar[i]=get_float(&p);
    uint32_t ndl=get_u32(&p),tts=get_u32(&p);
    if(ndl>14400 || tts>59940)return OSTC4_CHECKPOINT_INVALID;
    saved.ndl_seconds=(int)ndl;saved.tts_seconds=(int)tts;
    for(int i=0;i<OSTC4_STOPS;++i) {
        saved.stop_seconds[i]=(unsigned short)(p[0]|((unsigned)p[1]<<8));p+=2;
    }
    saved.tissues=*p++;saved.ndl=*p++;saved.tts=*p++;saved.ceiling=*p++;saved.stops=*p++;
    saved.gf_pair.low=*p++;saved.gf_pair.high=*p++;
    if(*p>1)return OSTC4_CHECKPOINT_INVALID;
    saved.gf_locked=*p++;
    if(!checkpoint_observation_valid(&saved,surface,max_depth))return OSTC4_CHECKPOINT_INVALID;
    if(!recovery || !recovery->time_known)return OSTC4_RESTART_TIME_UNKNOWN;
    if(recovery->restart_seconds<saved.timestamp_seconds)return OSTC4_RESTART_TIME_UNKNOWN;
    if(recovery->restart_seconds-saved.timestamp_seconds>recovery->max_age_seconds)
        return OSTC4_CHECKPOINT_STALE;
    if((recovery->count && !recovery->history) ||
       (recovery->restart_seconds>saved.timestamp_seconds && !recovery->count) ||
       (recovery->count && recovery->history[recovery->count-1].timestamp_seconds!=recovery->restart_seconds))
        return OSTC4_RESTART_EXPOSURE_UNKNOWN;
    uint32_t previous=saved.timestamp_seconds;
    for(size_t i=0;i<recovery->count;++i) {
        const ostc4_sample *sample=&recovery->history[i];
        if(sample->timestamp_seconds<=previous || sample->timestamp_seconds-previous>3600 ||
           !checkpoint_number(sample->absolute_pressure_bar,surface,surface+6))
            return OSTC4_RESTART_EXPOSURE_REJECTED;
        previous=sample->timestamp_seconds;
    }
    /* Restore only persistent physiology/settings. Planner scratch other than
     * GF-low depth is rebuilt on each calculation; see pinned-source audit. */
    ostc4_exposure_initialize(&(ostc4_initial){saved.timestamp_seconds,surface});
    life.pressure_ambient_bar=saved.absolute_pressure_bar;
    life.max_depth_meter=max_depth;
    life.dive_time_seconds_without_surface_time=saved.dive_seconds;
    memcpy(life.tissue_nitrogen_bar,saved.nitrogen_bar,sizeof(saved.nitrogen_bar));
    memcpy(life.tissue_helium_bar,saved.helium_bar,sizeof(saved.helium_bar));
    settings.internal__pressure_first_stop_ambient_bar_as_upper_limit_for_gf_low_otherwise_zero=saved.gf_anchor_ambient_bar;
    settings.gf_low=saved.gf_pair.low;settings.gf_high=saved.gf_pair.high;
    gGF_low_depth_bar=saved.gf_low_depth_bar;current=saved;
    for(size_t i=0;i<recovery->count;++i)
        if(ostc4_exposure_advance(&recovery->history[i])!=OSTC4_OK) {
            invalidate_exposure();return OSTC4_RESTART_EXPOSURE_REJECTED;
        }
    return OSTC4_RESTORED;
}
const char *ostc4_restart_status_name(ostc4_restart_status status)
{
    switch(status) {
    case OSTC4_RESTART_CONFIGURATION_UNKNOWN:return "Unknown checkpoint configuration history";
    case OSTC4_RESTORED:return "Recovered: complete exposure";
    case OSTC4_CHECKPOINT_CORRUPT:return "Corrupt checkpoint";
    case OSTC4_CHECKPOINT_INCOMPATIBLE:return "Incompatible checkpoint";
    case OSTC4_CHECKPOINT_STALE:return "Stale checkpoint";
    case OSTC4_RESTART_TIME_UNKNOWN:return "Unknown restart time";
    case OSTC4_RESTART_EXPOSURE_UNKNOWN:return "Unknown restart exposure";
    case OSTC4_CHECKPOINT_INVALID:return "Invalid checkpoint state";
    case OSTC4_RESTART_EXPOSURE_REJECTED:return "Rejected restart exposure";
    }
    return "Unknown restart failure";
}
#endif
